

# **A Comprehensive Guide to Continuous RMT Transmission with Large-Scale PSRAM Buffering on ESP32**

## **Section 1: Executive Summary and Feasibility Assessment**

### **1.1. Direct Feasibility Analysis**

The objective of implementing a continuous, high-fidelity data transmission using the ESP32's Remote Control Transceiver (RMT) peripheral, sourced from a large data buffer residing in external PSRAM, is not only feasible but represents a powerful and common high-performance design pattern for the ESP32 family of System-on-Chips (SoCs). This architecture is essential for applications requiring the generation of complex, non-repetitive waveforms that are too large to fit within the limited internal SRAM, such as driving large LED displays or emulating sophisticated digital protocols.

However, the specific implementation strategy outlined in the query—transferring data from a PSRAM-based queue directly into the RMT's hardware buffers from within an RMT Interrupt Service Routine (ISR)—is **fundamentally unworkable and will invariably lead to system instability, unpredictable behavior, and fatal exceptions**. This report will deconstruct the underlying hardware and software constraints that render this direct-ISR approach invalid and present a robust, production-ready alternative architecture that achieves the desired outcome safely and efficiently.

### **1.2. The Core Technical Hurdles**

The proposed architecture fails due to two non-negotiable constraints inherent in the ESP32's design, particularly concerning the interaction between real-time peripherals, ISRs, and external memory.

1. **ISR Context and PSRAM Inaccessibility:** PSRAM is not on-die memory; it is an external component accessed via a cached SPI bus. This bus and its associated cache are shared resources, most notably with the SPI flash memory that stores the application code. Critical system tasks, such as the Wi-Fi stack or flash driver, must occasionally perform operations (e.g., writing to flash) that require the CPU cache to be disabled. During these brief but critical windows, any attempt by any process, including an ISR, to access a cached memory region like PSRAM will result in a fatal Cache disabled but cached memory region got accessed exception, crashing the system. Because the timing of these cache-disabling events is non-deterministic from an application's perspective, accessing PSRAM from any ISR is inherently unsafe and must be avoided.1  
2. **Peripheral DMA Memory Constraints:** The ESP32's Direct Memory Access (DMA) controllers, which are leveraged by peripherals like RMT to offload data transfer from the CPU, have strict limitations on their source memory. The DMA engine can only access data from a specific pool of **internal SRAM**. In the ESP-IDF framework, this memory must be explicitly allocated using the MALLOC\_CAP\_DMA flag. The ESP-IDF documentation and hardware reference manuals are unequivocal: PSRAM is not part of this DMA-capable region and cannot be used as a direct source for peripheral DMA transfers.4

### **1.3. The Recommended Architecture: An Asynchronous, Task-Based Pipeline**

To successfully and robustly implement continuous RMT transmission from a PSRAM source, it is necessary to decouple the data producer (the application logic managing the PSRAM buffer) from the ultimate data consumer (the RMT peripheral). This is achieved with a task-based, multi-buffer pipeline architecture that honors the hardware constraints.

The recommended architecture consists of the following key components:

1. **Large Data Source:** A large buffer or queue holding the complete transmission data, allocated in **PSRAM**.  
2. **Intermediate DMA Buffers:** A set of two or more "ping-pong" buffers allocated in **internal, DMA-capable SRAM**. These act as a high-speed staging area for the RMT peripheral.  
3. **Data Mover Task:** A dedicated, high-priority FreeRTOS task that serves as the central orchestrator. Its sole responsibility is to move data chunks from PSRAM into the internal DMA buffers.  
4. **Asynchronous Memory Copy:** On supported chips (e.g., ESP32-S3), the esp\_async\_memcpy API is used by the mover task to perform the PSRAM-to-SRAM copy using a separate DMA engine, minimizing CPU blocking and maximizing efficiency.8  
5. **RMT Peripheral and DMA:** The RMT transmitter is configured to use its own DMA engine to read data exclusively *from* the internal SRAM buffers.  
6. **ISR-to-Task Synchronization:** A lightweight FreeRTOS primitive, such as a binary semaphore, is used for communication. The RMT's on\_trans\_done ISR signals the semaphore upon completing a transmission, which unblocks the Data Mover Task, notifying it that an internal buffer is now free to be refilled.

This architecture respects all hardware limitations by ensuring that:

* No PSRAM access ever occurs within an ISR context.  
* The RMT's DMA controller is only ever fed data from valid, internal DMA-capable memory.  
* The system remains responsive and stable, as all time-consuming operations are managed within a preemptible task context, not a time-critical ISR.

This report will now proceed to explore each of these components in exhaustive detail, providing the technical foundation, implementation strategies, and complete code examples necessary to build this robust system.

## **Section 2: Mastering Continuous RMT Transmission**

The RMT peripheral is a remarkably versatile tool in the ESP32's arsenal, designed for far more than its original purpose of infrared remote control. It functions as a high-precision, programmable signal generator capable of creating complex, arbitrary, and non-periodic digital waveforms, making it ideal for a vast range of protocols from driving addressable LEDs to emulating custom serial interfaces.10 Achieving a continuous, glitch-free output stream, especially when the data source is large, requires a sophisticated understanding of its driver models and operational modes.

### **2.1. The RMT Peripheral: A High-Precision Signal Generator**

At its core, the RMT peripheral operates on a sequence of "symbols." In the modern ESP-IDF v5.x driver, this is represented by the rmt\_symbol\_word\_t structure, which is a 32-bit word encoding two distinct pulse definitions. Each pulse definition contains a 15-bit duration and a 1-bit logic level.13 The duration is measured in ticks of the RMT's source clock, which is derived from a system clock (typically the 80 MHz APB clock) via a configurable divider (

clk\_div). This architecture allows for pulse width resolution down to 12.5 ns (1 / 80 MHz).15 The RMT hardware reads these symbols from a dedicated block of internal RAM and translates them into a timed sequence of high and low levels on a GPIO pin.

### **2.2. A Tale of Two Drivers: Legacy (v4.x) vs. Modern (v5.x) ESP-IDF**

A developer exploring RMT will encounter examples and documentation spanning two distinct driver generations. Understanding their differences is crucial, as older methods are more complex and less safe, while the modern driver provides a robust, abstracted interface that is strongly recommended for new designs.

#### **2.2.1. Legacy Driver (ESP-IDF v4.x and earlier)**

The legacy driver is characterized by direct register manipulation and a more manual configuration process. Key features include:

* **Configuration:** A single, large rmt\_config\_t structure is used to configure the channel, GPIO, memory blocks, and other parameters.12  
* **Interrupt Handling:** Developers often implement "double buffering" by setting a threshold interrupt with rmt\_set\_tx\_thr\_intr\_en(). This fires an ISR when the RMT's internal buffer is, for example, half-empty. The ISR is then responsible for directly writing new rmt\_item32\_t data into the other half of the RMT's dedicated memory block, often by accessing the RMTMEM structure directly.18  
* **Complexity and Risk:** This approach offers fine-grained control but is fraught with peril. It requires the developer to manually manage memory pointers and buffer states within an ISR, a practice that is highly prone to timing errors and race conditions. Furthermore, if the data to be written into the buffer originates from an unsafe location like PSRAM, it will lead to the crashes described previously.

#### **2.2.2. Modern Driver (ESP-IDF v5.x and later)**

The modern driver, introduced around ESP-IDF v5.0, represents a significant architectural improvement, emphasizing safety, abstraction, and ease of use.

* **Resource Allocation:** Peripherals are treated as objects. An RMT channel is allocated and configured by calling rmt\_new\_tx\_channel(), which returns an opaque handle (rmt\_channel\_handle\_t).10 This prevents direct, unsafe access to hardware registers.  
* **Transaction-Based Operations:** Data is sent using the rmt\_transmit() function. This function takes an encoder handle, a pointer to the source data, and the data size. It manages the entire process of encoding the data into RMT symbols and feeding them to the hardware.21  
* **Event Callbacks:** Instead of low-level threshold interrupts, the modern driver uses an event-driven model. The developer registers a callback structure, rmt\_tx\_event\_callbacks\_t, which includes a function pointer for on\_trans\_done. This callback is invoked safely from the driver's ISR when an entire transaction has completed.10 This is the primary mechanism for creating a continuous data pipeline.

The following table summarizes the key differences, guiding developers toward the modern, more robust API.

| Feature | Legacy ESP-IDF (v4.x) | Modern ESP-IDF (v5.x) | Architectural Implications |
| :---- | :---- | :---- | :---- |
| **Initialization** | rmt\_config() with a monolithic rmt\_config\_t struct. | rmt\_new\_tx\_channel() returning an opaque rmt\_channel\_handle\_t. | Modern API is safer, preventing direct hardware access and managing resources internally. |
| **Data Transmission** | rmt\_write\_items() with a pre-formatted array of rmt\_item32\_t. | rmt\_transmit() with a source data buffer and an rmt\_encoder\_handle\_t. | Modern API decouples data from its RMT representation, simplifying application code. |
| **Continuous Output** | Manual double buffering via rmt\_set\_tx\_thr\_intr\_en() and direct memory writes in a custom ISR. | Asynchronous rmt\_transmit() with trans\_queue\_depth \> 1 and an on\_trans\_done callback. | Modern approach is safer and more abstract, moving buffer management from the ISR to a task. |
| **Safety** | High risk of race conditions and unsafe memory access in custom ISRs. Prone to errors. | High. Driver internals are protected. Callbacks are well-defined. | The modern driver is designed to prevent common pitfalls, especially those related to ISRs. |

### **2.3. Strategies for Glitch-Free Continuous Transmission (Modern Driver)**

Using the modern v5.x driver, there are three primary methods to achieve continuous signal generation, each suited to different use cases.

#### **2.3.1. The Transaction Queue and on\_trans\_done Callback**

This is the most flexible and common method for streaming long, arbitrary data sequences. The implementation pattern is as follows:

1. When configuring the RMT channel, set trans\_queue\_depth in rmt\_tx\_channel\_config\_t to a value greater than 1 (e.g., 2 or more). This creates a software queue within the RMT driver to hold pending transactions.22  
2. Register an on\_trans\_done callback function using rmt\_tx\_register\_event\_callbacks(). This callback will be invoked from the RMT driver's internal ISR context each time a transaction passed to rmt\_transmit() completes.  
3. Initiate the stream by calling rmt\_transmit() one or more times to prime the queue. Because the queue has depth, these calls will return immediately without blocking.  
4. Inside the on\_trans\_done callback, perform the *minimal* required action: signal a higher-level application task (e.g., using xSemaphoreGiveFromISR) that a transmission has finished.  
5. The application task, upon receiving the signal, prepares the next chunk of data and calls rmt\_transmit() again to add it to the RMT driver's queue.

This creates a self-sustaining pipeline. While one data chunk is being transmitted by the hardware, the application task can be preparing the next one.

A critical consideration for this method is the potential for a small, non-deterministic gap between consecutive transmissions. The gap's duration is the sum of the latencies involved in the signaling chain: the RMT hardware finishing, the ISR firing, the semaphore being given, the FreeRTOS scheduler unblocking the waiting task, and that task finally calling rmt\_transmit() again. This latency can introduce jitter.22 For many protocols like addressable LEDs, this small gap occurs during the "reset" period and is harmless. However, for protocols that require a perfectly contiguous bitstream with no clock interruption, this method may be unsuitable.

#### **2.3.2. The Hardware Loop (loop\_count)**

For applications that require the perfect, gapless repetition of a fixed pattern (e.g., generating a stable carrier frequency or a repeating preamble), the hardware loop is the ideal solution.

* By setting rmt\_transmit\_config\_t::loop\_count to a positive integer or to \-1 (for infinite looping), the RMT hardware will automatically re-transmit the contents of its memory buffer without any CPU intervention.10  
* This method provides the most precise and stable periodic signal generation possible.  
* Its primary limitation is that it can only repeat the *same* pre-loaded data. It cannot be used to stream a long, non-repeating sequence of data. The on\_trans\_done event is not generated until the entire loop count is complete (and is never generated for an infinite loop), so it cannot be used to chain different data blocks together.

#### **2.3.3. The Streaming Encoder**

This is the most advanced and powerful technique, capable of generating truly seamless, arbitrary data streams without the inter-transaction gap of the callback method.

* The developer creates a custom encoder function that conforms to the rmt\_encoder\_t interface.11  
* This encoder function is called directly by the RMT driver's ISR whenever the hardware's internal buffer has space for more symbols.  
* The encoder's job is to take raw application data (e.g., bytes from a buffer), convert them into rmt\_symbol\_word\_t on the fly, and write them into the memory buffer provided by the ISR.  
* Because the RMT's internal memory block is finite, a large transmission will require the encoder to be called multiple times. The encoder must therefore be stateful, keeping track of its position in the source data. When it has filled the provided space but has not finished encoding the entire source data, it returns a state of RMT\_ENCODING\_MEM\_FULL. The driver will then transmit the current buffer and call the encoder again later to get the next part.11  
* This "ping-pong" interaction between the driver ISR and the custom encoder allows for the continuous generation of RMT symbols, fed directly into the hardware stream.

However, this powerful technique comes with the same critical constraint discussed in Section 1: the encoder function runs in an ISR context. Therefore, both the encoder function itself (marked with IRAM\_ATTR) and the source data it reads from **must reside in internal RAM**. It cannot directly access data from PSRAM.11 This reinforces the central thesis of this report: data must first be moved from PSRAM to a staging area in internal SRAM before it can be consumed by a real-time process like the RMT peripheral.

## **Section 3: A Deep Dive into PSRAM: Capabilities and Critical Constraints**

Pseudo-Static RAM (PSRAM) is a transformative technology for the ESP32 platform, enabling applications that would be impossible with the few hundred kilobytes of on-die SRAM alone. From complex graphical user interfaces to large-scale data logging, PSRAM provides the necessary memory expansion.24 However, using it effectively, especially in real-time systems, requires a deep understanding of its architecture, performance characteristics, and, most importantly, its stringent limitations.

### **3.1. PSRAM Architecture and Integration**

PSRAM is fundamentally Dynamic RAM (DRAM) that includes on-chip refresh circuitry, making it appear "pseudo-static" to the host microcontroller.26 On ESP32-family devices, it is an external chip connected via the SPI peripheral bus, often sharing pins with the main flash memory.24

The integration into the ESP32's memory space is handled by the Memory Management Unit (MMU). The MMU transparently maps the external PSRAM into the CPU's data address space, typically starting at a virtual address like 0x3F800000 on the original ESP32 or 0x3D000000 on the ESP32-S3.2 This allows the CPU to access PSRAM using standard pointer operations, as if it were internal memory. ESP-IDF can be configured to make this memory available through the standard

malloc() call, simplifying its use in application code.2

The physical interface has evolved across ESP32 generations. While older models used Quad-SPI (4-bit data bus), newer chips like the ESP32-S3 support much faster Octal-SPI (8-bit data bus) PSRAM, significantly improving bandwidth. The clock speed of this interface is also configurable, commonly at 40 MHz, 80 MHz, or even 120 MHz on the latest SoCs, which directly impacts access speed.1

### **3.2. Performance Characteristics: Speed, Latency, and the Cache**

The single most important factor governing PSRAM performance is the CPU's cache. All accesses to external memory (both flash and PSRAM) are routed through a 32 KB cache (on most ESP32 variants). The performance difference between a cache hit and a cache miss is dramatic:

* **Cache Hit:** If the requested data is already in the cache, the access is nearly as fast as accessing internal SRAM, taking only a few CPU cycles.30  
* **Cache Miss:** If the data is not in the cache, the CPU must stall while the MMU initiates an SPI bus transaction to fetch a block of data (typically 32 bytes) from the external PSRAM chip. This process is orders of magnitude slower than a direct SRAM read.31

This behavior has profound implications for application performance. Accessing small, frequently used variables in PSRAM is highly efficient. However, performing large, sequential memory operations (reads or writes) that exceed the 32 KB cache size will lead to "cache thrashing," where new data being fetched constantly evicts older data. In this state, performance degrades to the raw throughput of the underlying SPI bus.

User-provided benchmarks illustrate this effect perfectly. In one test, 32-bit read operations on small arrays (\< 16 KB) achieved speeds over 90 MB/s, as the entire operation was contained within the cache. When the array size increased to 64 KB and beyond, the performance plummeted to a sustained \~21 MB/s, reflecting the raw speed of the PSRAM bus after the cache was saturated.31 Similarly, another source quotes the theoretical maximum throughput of 80 MHz Quad-SPI PSRAM at 40 MB/s, but notes that real-world write performance is often halved due to the read-modify-write nature of cache line fills.30

### **3.3. The Two Unbreakable Rules of PSRAM for Real-Time Systems**

While the performance characteristics are important for optimization, two hard limitations are absolutely critical for system stability. Violating either of these rules will result in a non-functional system.

#### **3.3.1. Rule \#1: Thou Shalt Not Access PSRAM from an ISR (Directly)**

This is the paramount constraint that invalidates the user's proposed architecture. The reason is the shared cache between PSRAM and the code-holding flash memory.

1. **Shared Resource Conflict:** The Wi-Fi stack, Bluetooth stack, and the flash driver itself are high-priority system components. When they need to perform a write or erase operation on the main SPI flash chip, they must ensure no other process attempts to execute code from flash simultaneously.  
2. **Cache Disabling:** To guarantee this, these drivers execute a critical section that disables the CPU cache. This effectively freezes all code execution from flash. To prevent a deadlock where a CPU is waiting for the flash operation to finish but cannot execute the code to do so, the CPUs enter a tight spin-wait loop in IRAM until the flash operation completes and the cache is re-enabled.3  
3. **The ISR Trap:** If a peripheral interrupt (like from the RMT) fires during this cache-disabled window, its ISR handler (which should be in IRAM) will begin to execute. If this ISR handler then attempts to read or write any data located in PSRAM, the access will be routed through the MMU to the now-disabled cache controller. This triggers an unrecoverable hardware exception (Cache disabled but cached memory region got accessed), and the system crashes.1

Because the timing of flash operations is not predictable by the application, any direct access to PSRAM from an ISR creates a latent, non-deterministic bug that is guaranteed to manifest under real-world conditions (e.g., when Wi-Fi is actively transmitting). This is why the ESP-IDF documentation explicitly warns against this practice and why task stacks are, by default, never placed in PSRAM.2

#### **3.3.2. Rule \#2: Thou Shalt Not Give a PSRAM Buffer to a Peripheral's DMA**

This is the second, equally critical constraint. The ESP-IDF heap allocator uses a capabilities-based system to manage different memory types. A call to heap\_caps\_malloc() can request memory with specific attributes.6

* MALLOC\_CAP\_SPIRAM: Allocates memory from external PSRAM.2  
* MALLOC\_CAP\_DMA: Allocates memory from a special region of internal SRAM that is physically connected to the peripheral DMA bus.

The documentation is explicit: the MALLOC\_CAP\_DMA flag **excludes** any external PSRAM.6 There is no hardware path for the RMT peripheral's DMA engine to directly read data from the PSRAM's SPI bus. Its DMA controller can only access the internal DMA-capable memory pool.

Therefore, configuring an RMT channel with DMA enabled (with\_dma \= true) and then passing a PSRAM buffer pointer to rmt\_transmit() will simply fail. The DMA controller will not be able to fetch the data, resulting in no output or corrupted output. Any buffer intended for use with a peripheral DMA transaction *must* be allocated from internal memory with MALLOC\_CAP\_DMA.4

The following table provides a clear reference for these critical memory characteristics.

| Feature | Internal SRAM (non-DMA) | Internal SRAM (DMA-Capable) | External PSRAM |
| :---- | :---- | :---- | :---- |
| **Access Speed** | Highest (single-cycle CPU access) | Highest (single-cycle CPU access) | Lower (SPI bus, cache-dependent) |
| **Typical Size** | \~200-300 KB | \~100-160 KB (a subset of internal SRAM) | 4 MB \- 32 MB |
| **Peripheral DMA Access** | No | **Yes** | **No** |
| **ISR Access Safety** | Safe | Safe | **Unsafe** (due to cache disabling) |
| **Allocation Flag** | MALLOC\_CAP\_INTERNAL | MALLOC\_CAP\_DMA | MALLOC\_CAP\_SPIRAM |

These constraints collectively mandate an architecture that uses internal, DMA-capable SRAM as an intermediary between the large data store in PSRAM and the real-time RMT peripheral.

## **Section 4: The Recommended Architecture: A Resilient Task-Based Buffering Pipeline**

Given the fundamental hardware and software constraints outlined in the previous sections, a robust architecture must be designed to safely and efficiently bridge the gap between the large, slow, and ISR-unsafe PSRAM and the fast, real-time RMT peripheral. The optimal solution is an asynchronous, task-based pipeline that uses intermediate buffers in internal memory and relies on efficient synchronization primitives.

### **4.1. Architectural Blueprint**

This architecture decouples the various stages of the data flow, allowing them to operate in parallel and ensuring that no single component violates the system's operational rules. The data journey proceeds as follows:

1. **Data Source in PSRAM:** The entire, multi-megabyte data sequence to be transmitted is stored in a large buffer in PSRAM.  
2. **Staging in Internal SRAM:** A pair (or more) of smaller "ping-pong" buffers are allocated in the internal, DMA-capable region of SRAM. These will serve as the direct source for the RMT peripheral.  
3. **The Manager Task Initiates Copy:** A high-priority FreeRTOS task, the "Data Mover," identifies the next chunk of data required from the PSRAM source and a free internal buffer to copy it into.  
4. **Asynchronous PSRAM-to-SRAM Transfer:** The Manager Task initiates a memory copy from PSRAM to the chosen internal buffer. On modern chips like the ESP32-S3, this is done non-blockingly using the esp\_async\_memcpy API, which leverages a dedicated GDMA channel. The CPU is now free to perform other work.  
5. **Copy Completion Notification:** When the esp\_async\_memcpy operation completes, its completion callback (running in an ISR context) signals the Manager Task, typically via a semaphore.  
6. **Queueing the RMT Transaction:** Now that the internal buffer is filled with fresh data, the Manager Task calls rmt\_transmit(), passing a pointer to this internal buffer. The RMT's own DMA engine takes over, transmitting the data from the internal buffer in the background. This call is non-blocking as the RMT driver has a transaction queue.  
7. **Transmission Completion Notification:** When the RMT peripheral finishes transmitting the contents of the internal buffer, its on\_trans\_done callback fires.  
8. **Signaling Buffer Availability:** This minimal ISR does one thing: it signals the Manager Task (again, via a semaphore) to indicate that the internal buffer it just finished transmitting is now free and can be reused.  
9. **The Cycle Repeats:** The Manager Task, unblocked by the signal from the RMT ISR, now knows a buffer is free and loops back to step 3 to fetch the next chunk of data from PSRAM.

This continuous, pipelined process ensures the RMT peripheral is constantly fed with data without ever starving, while respecting all memory and ISR context limitations.

### **4.2. System Components in Detail**

A successful implementation requires careful configuration of each component in this pipeline.

#### **4.2.1. The Data Source (PSRAM)**

The large source buffer must be allocated in PSRAM. While the default malloc() can be configured to use PSRAM, it is best practice to be explicit. The allocation should be made using heap\_caps\_malloc() with the MALLOC\_CAP\_SPIRAM flag. This ensures the buffer is placed in external memory regardless of the system's default heap configuration.2

Example Allocation:  
uint8\_t\* psram\_source\_buffer \= (uint8\_t\*) heap\_caps\_malloc(LARGE\_BUFFER\_SIZE, MALLOC\_CAP\_SPIRAM);

#### **4.2.2. The Intermediate DMA Buffers (Internal SRAM)**

These are the most critical components of the bridge. They must be in internal memory and be DMA-capable.

* **Allocation:** They must be allocated using heap\_caps\_malloc() with the MALLOC\_CAP\_DMA flag. It is good practice to also include MALLOC\_CAP\_INTERNAL to be explicit.2  
* **Number:** A minimum of two buffers are required for a "ping-pong" scheme: while the RMT is transmitting from buffer A, the Manager Task is filling buffer B. Using three or more buffers can provide additional resilience against scheduling jitter.  
* **Size:** The size of these buffers is a critical performance tuning parameter. It must be large enough that the RMT peripheral does not run out of data while the next buffer is being filled from the slower PSRAM. This calculation is detailed in Section 6\.

Example Allocation (for an array of two buffers):  
dma\_buffers\[i\] \= heap\_caps\_malloc(DMA\_BUFFER\_SIZE, MALLOC\_CAP\_DMA | MALLOC\_CAP\_INTERNAL);

#### **4.2.3. The Data Mover Task (FreeRTOS)**

This task is the brain of the operation.

* **Creation:** It should be created using xTaskCreatePinnedToCore(). Pinning the task to a specific core (e.g., Core 1, if Wi-Fi is on Core 0\) prevents it from migrating and can reduce cache coherency overhead and contention with other high-priority system tasks.3  
* **Priority:** It should be assigned a very high priority, such as configMAX\_PRIORITIES \- 1, to ensure it is scheduled with minimal latency as soon as it is signaled by an ISR.35

#### **4.2.4. The Asynchronous Bridge (esp\_async\_memcpy)**

On ESP32-S3 and newer chips, the General Purpose DMA (GDMA) provides channels for memory-to-memory transfers. The esp\_async\_memcpy API provides a simple interface to this hardware.8

* **Benefit:** Using this API offloads the PSRAM-to-SRAM copy from the CPU. The Manager Task can initiate the copy and then block on a semaphore, yielding the CPU. Without this, the high-priority Manager Task would be stuck in a memcpy() loop, consuming 100% of its core's CPU time and potentially starving other tasks.  
* **Fallback for Older Chips:** On the original ESP32 or ESP32-S2, which lack a dedicated memory-to-memory DMA engine accessible via this API 37, the Manager Task must perform the copy manually using a standard  
  memcpy(). This is less efficient but still functional. The performance implications of this blocking operation must be carefully considered when sizing the DMA buffers.

### **4.3. Synchronization: The Glue of the System**

Efficient and safe communication between the ISRs and the Manager Task is paramount.

* **Binary Semaphores:** These are the ideal tool for this purpose. They are lightweight and designed for ISR-to-task signaling. Two semaphores would typically be used in this architecture:  
  1. rmt\_tx\_done\_sem: Given by the RMT on\_trans\_done ISR and taken by the Manager Task to signal that an internal DMA buffer is now free.  
  2. memcpy\_done\_sem: Given by the esp\_async\_memcpy callback ISR and taken by the Manager Task to signal that the PSRAM-to-SRAM copy is complete and the buffer is ready for transmission.  
* **Queues:** For managing the pool of internal DMA buffers, a FreeRTOS queue (xQueueHandle) is an excellent choice. The queue would store the pointers to the available buffers.  
  * When the RMT on\_trans\_done ISR fires, it places the pointer to the now-free buffer back into the queue using xQueueSendFromISR().  
  * The Manager Task waits to receive a buffer pointer from the queue using xQueueReceive(). This elegantly manages the pool of buffers and provides a natural blocking point for the task.

This robust, multi-stage, asynchronous pipeline provides a safe, efficient, and scalable solution for continuous RMT transmission from a large PSRAM data source, correctly navigating the complex hardware constraints of the ESP32 platform.

## **Section 5: Implementation Guide and Annotated Code**

This section provides a practical, annotated code implementation of the recommended task-based buffering pipeline. The example is written for the ESP-IDF v5.x framework and targets an ESP32-S3, which supports esp\_async\_memcpy. Notes will be provided for adapting the code to older chips that lack this feature.

The code demonstrates the full data flow: initializing the system, managing a pool of DMA-capable buffers, and orchestrating the asynchronous transfer from a large PSRAM buffer to the RMT peripheral for continuous transmission.

### **5.1. Phase 1: System and Buffer Initialization**

This is the main entry point of the application. It sets up all the necessary components: PSRAM, RMT channel, DMA buffers, the asynchronous memory copy driver, and the FreeRTOS task and synchronization primitives that form the core of the pipeline.

C++

\#**include** \<stdio.h\>  
\#**include** \<string.h\>  
\#**include** "freertos/FreeRTOS.h"  
\#**include** "freertos/task.h"  
\#**include** "freertos/semphr.h"  
\#**include** "freertos/queue.h"  
\#**include** "esp\_system.h"  
\#**include** "esp\_log.h"  
\#**include** "driver/rmt\_tx.h"  
\#**include** "esp\_check.h"  
\#**include** "esp\_psram.h"  
\#**include** "esp\_async\_memcpy.h"

static const char \*TAG \= "RMT\_PSRAM\_STREAM";

// \--- Configuration Constants \---  
\#**define** RMT\_TX\_GPIO\_NUM         GPIO\_NUM\_4  
\#**define** RMT\_RESOLUTION\_HZ       10000000 // 10 MHz resolution, 1 tick \= 100 ns  
\#**define** RMT\_TRANS\_QUEUE\_DEPTH   4        // RMT driver transaction queue size

\#**define** PSRAM\_DATA\_SIZE         (1 \* 1024 \* 1024\) // 1MB of data in PSRAM  
\#**define** NUM\_DMA\_BUFFERS         3                 // Use 3 internal DMA buffers for pipelining  
\#**define** DMA\_BUFFER\_SIZE         4092              // Size of each DMA buffer in bytes. Must be a multiple of 4\.

// \--- System Handles \---  
static rmt\_channel\_handle\_t rmt\_tx\_channel \= NULL;  
static rmt\_encoder\_handle\_t bytes\_encoder \= NULL;  
static async\_memcpy\_handle\_t mcp\_handle \= NULL;

// \--- Synchronization Primitives \---  
static QueueHandle\_t free\_dma\_buffers\_queue;     // Holds pointers to free DMA buffers  
static QueueHandle\_t filled\_dma\_buffers\_queue;   // Holds pointers to DMA buffers filled with data, ready for RMT

// \--- Buffers \---  
static uint8\_t \*psram\_source\_buffer \= NULL;      // Large data source in PSRAM  
static uint8\_t \*dma\_buffers;    // Array of pointers to internal DMA buffers

// Forward declaration of the data mover task  
void data\_mover\_task(void \*arg);

// \--- Main Application Entry \---  
extern "C" void app\_main(void)  
{  
    ESP\_LOGI(TAG, "Initializing PSRAM...");  
    // On ESP32-S3, PSRAM is initialized automatically if enabled in menuconfig.  
    // We can check if it's available.  
    if (\!esp\_psram\_is\_initialized()) {  
        ESP\_LOGE(TAG, "PSRAM not available or initialized\!");  
        return;  
    }  
    size\_t psram\_size \= esp\_psram\_get\_size();  
    ESP\_LOGI(TAG, "PSRAM initialized. Size: %d bytes", psram\_size);

    // 1\. Allocate the large source buffer in PSRAM  
    ESP\_LOGI(TAG, "Allocating large source buffer in PSRAM...");  
    psram\_source\_buffer \= (uint8\_t \*)heap\_caps\_malloc(PSRAM\_DATA\_SIZE, MALLOC\_CAP\_SPIRAM);  
    if (\!psram\_source\_buffer) {  
        ESP\_LOGE(TAG, "Failed to allocate PSRAM source buffer\!");  
        return;  
    }  
    // Fill the source buffer with some pattern for demonstration  
    for (int i \= 0; i \< PSRAM\_DATA\_SIZE; i++) {  
        psram\_source\_buffer\[i\] \= (i % 256);  
    }  
    ESP\_LOGI(TAG, "PSRAM source buffer allocated and filled.");

    // 2\. Allocate the internal DMA-capable buffers  
    ESP\_LOGI(TAG, "Allocating internal DMA buffers...");  
    for (int i \= 0; i \< NUM\_DMA\_BUFFERS; i++) {  
        dma\_buffers\[i\] \= (uint8\_t \*)heap\_caps\_malloc(DMA\_BUFFER\_SIZE, MALLOC\_CAP\_DMA | MALLOC\_CAP\_INTERNAL);  
        if (\!dma\_buffers\[i\]) {  
            ESP\_LOGE(TAG, "Failed to allocate DMA buffer %d", i);  
            return;  
        }  
    }  
    ESP\_LOGI(TAG, "%d internal DMA buffers allocated.", NUM\_DMA\_BUFFERS);

    // 3\. Initialize the asynchronous memory copy driver  
    ESP\_LOGI(TAG, "Installing async memcpy driver...");  
    async\_memcpy\_config\_t mcp\_config \= ASYNC\_MEMCPY\_DEFAULT\_CONFIG();  
    mcp\_config.backlog \= NUM\_DMA\_BUFFERS; // Allow queuing up to NUM\_DMA\_BUFFERS copies  
    ESP\_ERROR\_CHECK(esp\_async\_memcpy\_install(\&mcp\_config, \&mcp\_handle));

    // 4\. Initialize the RMT Transmitter  
    ESP\_LOGI(TAG, "Initializing RMT TX Channel...");  
    rmt\_tx\_channel\_config\_t tx\_chan\_config \= {  
       .gpio\_num \= RMT\_TX\_GPIO\_NUM,  
       .clk\_src \= RMT\_CLK\_SRC\_DEFAULT,  
       .resolution\_hz \= RMT\_RESOLUTION\_HZ,  
       .mem\_block\_symbols \= 64, // ESP-IDF will automatically increase this if DMA is used  
       .trans\_queue\_depth \= RMT\_TRANS\_QUEUE\_DEPTH,  
       .flags \= {  
           .with\_dma \= true, // Enable the DMA backend  
        },  
    };  
    ESP\_ERROR\_CHECK(rmt\_new\_tx\_channel(\&tx\_chan\_config, \&rmt\_tx\_channel));

    // Create a simple bytes encoder  
    rmt\_bytes\_encoder\_config\_t bytes\_encoder\_config \= {  
       .bit0 \= {.level0 \= 1,.duration0 \= 2,.level1 \= 0,.duration1 \= 8 }, // 200ns high, 800ns low  
       .bit1 \= {.level0 \= 1,.duration0 \= 8,.level1 \= 0,.duration1 \= 2 }, // 800ns high, 200ns low  
    };  
    ESP\_ERROR\_CHECK(rmt\_new\_bytes\_encoder(\&bytes\_encoder\_config, \&bytes\_encoder));

    ESP\_ERROR\_CHECK(rmt\_enable(rmt\_tx\_channel));  
    ESP\_LOGI(TAG, "RMT Channel enabled.");

    // 5\. Create FreeRTOS Queues for buffer management  
    free\_dma\_buffers\_queue \= xQueueCreate(NUM\_DMA\_BUFFERS, sizeof(uint8\_t \*));  
    filled\_dma\_buffers\_queue \= xQueueCreate(NUM\_DMA\_BUFFERS, sizeof(uint8\_t \*));

    // 6\. Create and start the data mover task  
    ESP\_LOGI(TAG, "Starting data mover task...");  
    xTaskCreatePinnedToCore(data\_mover\_task, "data\_mover\_task", 4096, NULL, 5, NULL, 1);  
}

### **5.2. Phase 2: The Minimalist ISR Callbacks**

The architecture uses two ISR-based callbacks: one for esp\_async\_memcpy completion and one for RMT transaction completion. Both must be minimal, IRAM-safe, and do nothing more than signal a task via a FreeRTOS primitive. Here, they will add buffer pointers to the appropriate queues.

C++

// Callback executed when an RMT transaction finishes  
static IRAM\_ATTR bool rmt\_tx\_done\_callback(rmt\_channel\_handle\_t tx\_chan, const rmt\_tx\_done\_event\_data\_t \*edata, void \*user\_ctx)  
{  
    // This callback is fired from an ISR.  
    // We get the pointer to the buffer that was just transmitted from the 'edata' structure.  
    // We then send this pointer to the 'free\_dma\_buffers\_queue' to signal that it's available for reuse.  
    QueueHandle\_t free\_queue \= (QueueHandle\_t)user\_ctx;  
    BaseType\_t task\_woken \= pdFALSE;  
    xQueueSendFromISR(free\_queue, \&edata-\>tx\_symbol, \&task\_woken);  
    return task\_woken \== pdTRUE;  
}

// Callback executed when an async\_memcpy operation finishes  
static IRAM\_ATTR bool async\_memcpy\_done\_callback(async\_memcpy\_handle\_t mcp, async\_memcpy\_event\_t \*event, void \*user\_data)  
{  
    // This callback is also fired from an ISR.  
    // We get the pointer to the buffer that was the destination of the copy.  
    // We send this pointer to the 'filled\_dma\_buffers\_queue' to signal it's ready for RMT.  
    QueueHandle\_t filled\_queue \= (QueueHandle\_t)user\_data;  
    BaseType\_t task\_woken \= pdFALSE;  
    // The destination buffer pointer needs to be retrieved from the event data.  
    // For this example, we assume the user\_data for the memcpy call was the destination pointer.  
    // A more robust implementation might pass a struct with more context.  
    uint8\_t\* dest\_buffer \= (uint8\_t\*)event-\>user\_data;  
    xQueueSendFromISR(filled\_queue, \&dest\_buffer, \&task\_woken);  
    return task\_woken \== pdTRUE;  
}

*Note on async\_memcpy callback*: The above example simplifies how the destination buffer pointer is retrieved. A more robust method would involve passing a context structure to esp\_async\_memcpy that contains the destination pointer, which is then retrieved from event-\>user\_data. For this example, we will pass the destination buffer pointer itself as the cb\_args.

### **5.3. Phase 3: The Data Mover Task Logic**

This task is the heart of the pipeline. It orchestrates the entire flow, managing the buffers and ensuring the RMT peripheral is continuously supplied with data.

C++

void data\_mover\_task(void \*arg)  
{  
    // Register the RMT TX done callback  
    rmt\_tx\_event\_callbacks\_t cbs \= {  
       .on\_trans\_done \= rmt\_tx\_done\_callback,  
    };  
    ESP\_ERROR\_CHECK(rmt\_tx\_register\_event\_callbacks(rmt\_tx\_channel, \&cbs, free\_dma\_buffers\_queue));

    // Prime the free buffers queue with all available DMA buffers  
    for (int i \= 0; i \< NUM\_DMA\_BUFFERS; i++) {  
        xQueueSend(free\_dma\_buffers\_queue, \&dma\_buffers\[i\], 0);  
    }

    size\_t psram\_data\_offset \= 0;  
    uint8\_t \*current\_dma\_buffer \= NULL;

    // \--- Initial Kick-off: Fill the first few buffers \---  
    for (int i \= 0; i \< NUM\_DMA\_BUFFERS \- 1; i++) {  
        // 1\. Get a free DMA buffer  
        xQueueReceive(free\_dma\_buffers\_queue, \&current\_dma\_buffer, portMAX\_DELAY);  
          
        // 2\. Start async copy from PSRAM to the DMA buffer  
        size\_t copy\_size \= (psram\_data\_offset \+ DMA\_BUFFER\_SIZE \> PSRAM\_DATA\_SIZE)? (PSRAM\_DATA\_SIZE \- psram\_data\_offset) : DMA\_BUFFER\_SIZE;  
        ESP\_ERROR\_CHECK(esp\_async\_memcpy(mcp\_handle, current\_dma\_buffer, psram\_source\_buffer \+ psram\_data\_offset, copy\_size, async\_memcpy\_done\_callback, filled\_dma\_buffers\_queue));  
        psram\_data\_offset \= (psram\_data\_offset \+ copy\_size) % PSRAM\_DATA\_SIZE;  
    }

    // \--- Main Pipeline Loop \---  
    while (true) {  
        // 3\. Wait for a buffer to be filled by async\_memcpy  
        xQueueReceive(filled\_dma\_buffers\_queue, \&current\_dma\_buffer, portMAX\_DELAY);

        // 4\. Transmit the filled buffer with RMT  
        rmt\_transmit\_config\_t transmit\_config \= {  
           .loop\_count \= 0, // No loop  
        };  
        ESP\_ERROR\_CHECK(rmt\_transmit(rmt\_tx\_channel, bytes\_encoder, current\_dma\_buffer, DMA\_BUFFER\_SIZE, \&transmit\_config));

        // \--- Concurrently, start filling the next free buffer \---  
          
        // 1\. Get the next free DMA buffer (this will block until the RMT ISR returns one)  
        uint8\_t\* next\_free\_buffer;  
        xQueueReceive(free\_dma\_buffers\_queue, \&next\_free\_buffer, portMAX\_DELAY);

        // 2\. Start async copy for the next chunk  
        size\_t copy\_size \= (psram\_data\_offset \+ DMA\_BUFFER\_SIZE \> PSRAM\_DATA\_SIZE)? (PSRAM\_DATA\_SIZE \- psram\_data\_offset) : DMA\_BUFFER\_SIZE;  
        ESP\_ERROR\_CHECK(esp\_async\_memcpy(mcp\_handle, next\_free\_buffer, psram\_source\_buffer \+ psram\_data\_offset, copy\_size, async\_memcpy\_done\_callback, filled\_dma\_buffers\_queue));  
        psram\_data\_offset \= (psram\_data\_offset \+ copy\_size) % PSRAM\_DATA\_SIZE; // Wrap around for continuous stream  
    }  
}

Adaptation for chips without esp\_async\_memcpy (e.g., original ESP32):  
In the data\_mover\_task, the call to esp\_async\_memcpy would be replaced with a standard memcpy. The task would perform the copy itself, blocking the CPU. There would be no async\_memcpy\_done\_callback. After the memcpy completes, the task would manually xQueueSend the buffer pointer to the filled\_dma\_buffers\_queue. This increases CPU load and makes the timing calculations in the next section even more critical.

### **5.4. Table: Key sdkconfig Parameters for High-Performance RMT/PSRAM**

Properly configuring the project via idf.py menuconfig is essential for both functionality and performance. The following table highlights the most critical parameters for this architecture.

| Parameter (menuconfig path) | Description | Recommended Setting | Rationale/Impact |
| :---- | :---- | :---- | :---- |
| **Component config \> ESP PSRAM \> Support for external RAM** | Enables PSRAM support globally. | (X) Enabled | Required for any PSRAM functionality. |
| **Component config \> ESP PSRAM \> SPI RAM config \> SPI RAM access method** | Controls how PSRAM is integrated into the heap. | Make RAM allocatable using heap\_caps\_malloc | Provides explicit control via MALLOC\_CAP\_SPIRAM, preventing accidental allocations. |
| **Component config \> ESP PSRAM \> SPI RAM config \> Type of SPI RAM chip in use** | Selects the timing and initialization sequence for the specific PSRAM chip. | Match your hardware (e.g., Auto-detect, ESP-PSRAM64) | Incorrect setting will cause PSRAM initialization to fail.38 |
| **Component config \> ESP PSRAM \> SPI RAM config \> Set RAM clock speed** | Sets the clock speed for the SPI interface to the PSRAM. | 80MHz (or 120MHz on S3 Octal) | Higher speed directly translates to higher memory throughput, reducing the memcpy time.24 |
| **Component config \> RMT \> RMT ISR IRAM Safe** | Places RMT driver ISR code and objects in internal RAM. | (X) Enabled | Prevents crashes if an RMT interrupt occurs during a flash cache-disabled operation.10 |
| **Serial flasher config \> Flash SPI speed** | Sets the clock speed for the SPI flash chip. | 80MHz | Faster flash access improves overall system performance, especially code execution speed.40 |
| **Compiler options \> Optimization Level** | Sets the compiler optimization level. | Optimize for performance (-O2) | Increases code execution speed, which can reduce scheduling latency.40 |
| **FreeRTOS \> Tickless Idle** | Power-saving feature that can affect timer precision. | Disabled for max real-time performance | While good for low-power, it can introduce minor timing variations. Disable if absolute timing is paramount. |
| **FreeRTOS \> Unicore** | Runs FreeRTOS and all tasks on a single core. | Disabled (Dual Core) | Essential for pinning the data mover task to one core and system tasks (like Wi-Fi) to another. |

## **Section 6: Performance Tuning and System Optimization**

With the robust architecture in place, the final step is to tune its parameters to ensure maximum performance and prevent data underruns, where the RMT peripheral becomes idle because a new buffer is not ready in time. This involves a quantitative analysis of the pipeline's timing and strategic use of system resources.

### **6.1. Bottleneck Analysis and Buffer Sizing**

The core constraint of the pipeline is that the time required to refill an internal DMA buffer (T\_refill) must be less than the time it takes for the RMT peripheral to transmit one buffer (T\_rmt\_consume). If T\_refill \>= T\_rmt\_consume, the RMT will eventually starve, causing a gap in the output signal.

The two key timings are:

1. **RMT Consumption Time (T\_rmt\_consume):** This is deterministic and easy to calculate. It depends on the size of the internal DMA buffers and the final bitrate of the RMT signal.  
   * RMT\_bitrate\_bps \= RMT\_RESOLUTION\_HZ / (duration0 \+ duration1) for one bit.  
   * T\_rmt\_consume \= (DMA\_BUFFER\_SIZE\_bytes \* 8\) / RMT\_bitrate\_bps  
2. **Buffer Refill Time (T\_refill):** This is the sum of all latencies in the refill part of the pipeline.  
   * T\_refill \= T\_isr\_latency \+ T\_scheduling\_latency \+ T\_memcpy  
   * T\_isr\_latency: The time from the hardware event to the ISR signaling the task. This is typically very short, in the microsecond range.  
   * T\_scheduling\_latency: The time for FreeRTOS to wake up the high-priority Data Mover Task. With high priority, this is also very short.  
   * T\_memcpy: The time taken to copy DMA\_BUFFER\_SIZE bytes from PSRAM to internal SRAM. This is the dominant factor.

The T\_memcpy value depends heavily on the method used:

* **Using esp\_async\_memcpy:** The operation is non-blocking. The "time" from the task's perspective is near-zero. The actual copy happens in the background. The critical path is now the *actual duration* of the hardware copy. The GDMA on ESP32-S3 is very fast, but still limited by the PSRAM's read speed. For 80MHz Octal PSRAM, this speed can be in the range of 40-60 MB/s.  
  * T\_memcpy\_hw \= DMA\_BUFFER\_SIZE\_bytes / PSRAM\_read\_speed\_bytes\_per\_sec  
* **Using CPU memcpy (fallback):** The operation is blocking. The time is governed by the CPU's ability to read from PSRAM, which is heavily influenced by the cache. For transfers larger than the cache, the speed falls back to the raw bus speed of \~20-40 MB/s.30 This time directly adds to the task's execution time.  
  * T\_memcpy\_cpu \= DMA\_BUFFER\_SIZE\_bytes / PSRAM\_read\_speed\_bytes\_per\_sec

Practical Sizing Strategy:  
The primary goal is to choose a DMA\_BUFFER\_SIZE that makes T\_rmt\_consume comfortably larger than the worst-case T\_refill.

1. **Estimate T\_refill:** Assume a worst-case PSRAM read speed (e.g., 20 MB/s) and add a safety margin for scheduling latency (e.g., 50-100 µs).  
2. **Calculate Minimum T\_rmt\_consume:** Set T\_rmt\_consume\_min \= T\_refill \* 1.5 (a 50% safety margin).  
3. **Calculate Minimum DMA\_BUFFER\_SIZE:** Rearrange the formula:  
   * DMA\_BUFFER\_SIZE\_min\_bytes \= (T\_rmt\_consume\_min \* RMT\_bitrate\_bps) / 8

Choosing a buffer size larger than this calculated minimum ensures the pipeline has slack and can withstand variations in system load and scheduling latency. Using three or more buffers instead of two provides even more of a cushion.

### **6.2. CPU Core Affinity and Interrupt Priority**

In a multi-core system like the ESP32, contention for resources can introduce unpredictable latency. Strategic core placement is a powerful optimization tool.

* **Isolate Real-Time Tasks:** The Wi-Fi and Bluetooth stacks are complex and can impose significant, time-variable loads on the CPU core they run on (typically Core 0 by default).35 To protect the real-time RMT data pipeline from this interference, both the Data Mover Task and the RMT interrupt handler should be pinned to the other core (Core 1).3  
  * The task is pinned via xTaskCreatePinnedToCore().  
  * The RMT interrupt's affinity is typically determined by the core that installs the driver (rmt\_new\_tx\_channel). By running the setup code from a task pinned to Core 1, the interrupt will also be serviced on Core 1\.41  
* **Interrupt Priority:** The RMT interrupt priority can be set in rmt\_tx\_channel\_config\_t::intr\_priority. It should be set to a high level (e.g., 3 or ESP\_INTR\_FLAG\_LEVEL3) to ensure it preempts most application-level task code. However, it should not be set higher than critical system interrupts (like the FreeRTOS tick or Wi-Fi interrupts) unless absolutely necessary, to avoid starving the rest of the system.42

### **6.3. Maximizing Memory Throughput**

The speed of the PSRAM-to-SRAM copy is often the primary bottleneck. Maximizing this throughput is key.

* **Hardware Configuration:** In menuconfig, set the PSRAM and Flash SPI speeds to their maximum supported values (e.g., 80 MHz). For chips like the ESP32-S3, ensure the PSRAM mode is set correctly to enable Octal mode if using an Octal PSRAM chip.24  
* **Data Alignment:** DMA engines operate most efficiently when transferring data that is aligned to their natural bus widths. When using esp\_async\_memcpy on an ESP32-S3, the psram\_trans\_align field in the configuration should be set to 16, 32, or 64\. This allows the driver to configure the DMA for burst transfers, which can significantly improve throughput compared to single-byte transfers.8 The source and destination pointers, as well as the transfer size, should adhere to this alignment.

### **6.4. IRAM/DRAM Placement**

Cache misses are a primary source of latency. To eliminate them in performance-critical code paths:

* **ISR Handlers:** All ISR code, including the RMT and async\_memcpy callbacks, **must** be placed in Internal RAM (IRAM) using the IRAM\_ATTR attribute. This is not an optimization but a requirement for stability, as it ensures the ISR can run even if the flash cache is disabled.11  
* **Critical Task Functions:** For the Data Mover Task, any frequently called, performance-sensitive functions can also be decorated with IRAM\_ATTR. This prevents them from being evicted from the cache and ensures they execute at maximum speed every time.  
* **Driver Objects:** The CONFIG\_RMT\_ISR\_IRAM\_SAFE option is crucial as it ensures that the RMT driver's internal state variables (its "object") are allocated in internal DRAM, preventing an ISR from attempting to access them if they were accidentally placed in PSRAM by the heap allocator.10

By systematically applying these tuning strategies—calculating buffer sizes, managing core affinity, maximizing memory bus speeds, and strategically placing code in IRAM—the performance of the task-based pipeline can be maximized to create a highly reliable, high-throughput streaming system.

## **Section 7: Conclusion and Best Practices**

The endeavor to stream large volumes of data from external PSRAM through the RMT peripheral for continuous transmission is a sophisticated challenge that pushes the boundaries of the ESP32's capabilities. The analysis confirms that while the goal is achievable, the initial architectural proposal of using an ISR to directly ferry data from PSRAM is unviable due to fundamental hardware constraints. The successful approach requires a paradigm shift from direct, synchronous control to an asynchronous, decoupled pipeline managed by a dedicated real-time task.

### **7.1. Summary of Findings**

The core of this investigation revealed two immutable rules governing high-performance design on the ESP32:

1. **PSRAM and ISRs are incompatible.** The shared nature of the SPI bus and cache controller means that PSRAM becomes inaccessible during critical system events like flash writes. Any attempt by an ISR to access PSRAM during these windows will cause a fatal system crash.  
2. **Peripheral DMA cannot source from PSRAM.** The RMT's DMA engine is hard-wired to access a specific region of internal SRAM. It has no physical or logical path to retrieve data directly from external PSRAM.

These constraints mandate the adoption of the recommended architecture: a task-based buffering pipeline. This design uses internal, DMA-capable SRAM buffers as a high-speed intermediary between the large PSRAM data store and the RMT peripheral. A high-priority FreeRTOS task orchestrates the data flow, using esp\_async\_memcpy (where available) for efficient, non-blocking transfers and lightweight semaphores for safe, low-latency communication with the RMT's interrupt handler. This architecture is not merely a workaround; it is the correct and robust design pattern for this class of problem on the ESP32 platform.

### **7.2. Key Architectural Takeaways for ESP32 High-Throughput Design**

For developers embarking on similar projects, the principles derived from this analysis serve as a set of essential best practices for building stable and performant real-time systems:

* **Isolate Peripherals from Slow/Unsafe Memory:** Never design a system where a real-time peripheral is directly dependent on a slow, non-deterministic, or ISR-unsafe memory source like PSRAM. Always employ intermediate buffers in fast, safe, internal SRAM as a decoupling mechanism.  
* **Keep Interrupt Service Routines Minimalist:** An ISR should be treated as a highly privileged, time-critical piece of code. Its sole purpose is to handle the immediate hardware event (e.g., clear an interrupt flag) and, if necessary, signal a waiting task. All data processing, memory copies, and complex logic must be deferred to a proper FreeRTOS task context.  
* **Respect and Leverage Memory Capabilities:** The ESP-IDF's capabilities-based heap allocator (heap\_caps\_malloc) is not a suggestion but a reflection of hard hardware boundaries. Understand and correctly use flags like MALLOC\_CAP\_DMA and MALLOC\_CAP\_SPIRAM to ensure buffers are placed in memory regions that are physically accessible to the intended hardware block.  
* **Embrace Asynchronous Operations:** Build systems around non-blocking APIs and callbacks. Functions like rmt\_transmit (with a transaction queue) and esp\_async\_memcpy enable the creation of efficient pipelines that maximize CPU availability. This allows the system to perform computation and data movement in parallel, dramatically improving overall throughput.  
* **Design for the Worst Case:** When dealing with shared resources—be it a memory bus, a CPU core, or a peripheral—always design for worst-case contention. Pin critical tasks and interrupts to a dedicated core to isolate them from interference (e.g., from the Wi-Fi stack), and size buffers with sufficient safety margins to handle unexpected scheduling latencies. This proactive approach is the key to transforming a system that works on the bench into one that is reliable in the field.

By adhering to these principles, developers can confidently harness the full power of the ESP32's advanced peripherals and memory architecture to create complex, high-performance embedded applications.

#### **Works cited**

1. Support for External RAM \- ESP32-S3 \- — ESP-IDF Programming Guide v5.4.2 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html)  
2. Support for External RAM \- ESP32 \- — ESP-IDF Programming Guide v5.4.2 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/external-ram.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/external-ram.html)  
3. IPC Interfering with RMT \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=24917](https://esp32.com/viewtopic.php?t=24917)  
4. Help with double buff QSPI DMA on ESP32 S3 \- How-to \- LVGL Forum, accessed July 12, 2025, [https://forum.lvgl.io/t/help-with-double-buff-qspi-dma-on-esp32-s3/21062](https://forum.lvgl.io/t/help-with-double-buff-qspi-dma-on-esp32-s3/21062)  
5. ESP32 I2S: With PSRAM present it should use MALLOC\_CAP\_DMA for DMA-based transfers · Issue \#429 · Makuna/NeoPixelBus \- GitHub, accessed July 12, 2025, [https://github.com/Makuna/NeoPixelBus/issues/429](https://github.com/Makuna/NeoPixelBus/issues/429)  
6. Heap Memory Allocation \- ESP32 \- — ESP-IDF Programming Guide v5.4.2 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/mem\_alloc.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/mem_alloc.html)  
7. Support for external RAM — ESP-IDF Programming Guide v4.1-dev-2071-gf91080637 documentation \- Read the Docs, accessed July 12, 2025, [https://espressif-docs.readthedocs-hosted.com/projects/esp-idf/en/latest/api-guides/external-ram.html](https://espressif-docs.readthedocs-hosted.com/projects/esp-idf/en/latest/api-guides/external-ram.html)  
8. The Async memcpy API \- ESP32-S3 \- — ESP-IDF Programming ..., accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/system/async\_memcpy.html](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/system/async_memcpy.html)  
9. Asynchronous Memory Copy \- ESP32-C5 \- — ESP-IDF Programming Guide latest documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/system/async\_memcpy.html](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/system/async_memcpy.html)  
10. Remote Control Transceiver (RMT) \- ESP32 \- — ESP-IDF Programming Guide v5.1 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-reference/peripherals/rmt.html)  
11. Remote Control Transceiver (RMT) \- ESP32 \- — ESP-IDF Programming Guide v5.4.2 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/rmt.html)  
12. Remote Control (RMT) \- ESP32 \- — ESP-IDF Programming Guide v4.4.3 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v4.4.3/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v4.4.3/esp32/api-reference/peripherals/rmt.html)  
13. RMT API — Xedge32 documentation \- Real Time Logic, accessed July 12, 2025, [https://realtimelogic.com/ba/ESP32/source/RMT.html](https://realtimelogic.com/ba/ESP32/source/RMT.html)  
14. Peripherals \- ESP32 \- — ESP-IDF Programming Guide v5.1 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/migration-guides/release-5.x/5.0/peripherals.html](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/migration-guides/release-5.x/5.0/peripherals.html)  
15. RMT — ESP-IDF Programming Guide v3.0-dev-1395-gb9c6175 documentation, accessed July 12, 2025, [https://my-esp-idf.readthedocs.io/en/latest/api-reference/peripherals/rmt.html](https://my-esp-idf.readthedocs.io/en/latest/api-reference/peripherals/rmt.html)  
16. RMT — ESP-IDF Programming Guide v3.2.2-143-gca1e5e5bc documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v3.2.3/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v3.2.3/api-reference/peripherals/rmt.html)  
17. ESP32: Sending short pulses with the RMT at Buildlog.Net Blog, accessed July 12, 2025, [https://www.buildlog.net/blog/2017/11/esp32-sending-short-pulses-with-the-rmt/](https://www.buildlog.net/blog/2017/11/esp32-sending-short-pulses-with-the-rmt/)  
18. ESP32 / RMT loosing ticks / \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=7365](https://esp32.com/viewtopic.php?t=7365)  
19. Changes to RMT driver implementation? \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=13606](https://esp32.com/viewtopic.php?t=13606)  
20. ESP32 / RMT loosing ticks, accessed July 12, 2025, [https://www.esp32.com/viewtopic.php?t=7365](https://www.esp32.com/viewtopic.php?t=7365)  
21. Remote Control Transceiver (RMT) \- ESP32 \- — ESP-IDF Programming Guide v5.0 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/rmt.html)  
22. Continuous RMT transmissions (IDFGH-11922) · Issue \#13003 · espressif/esp-idf \- GitHub, accessed July 12, 2025, [https://github.com/espressif/esp-idf/issues/13003](https://github.com/espressif/esp-idf/issues/13003)  
23. Remote Control Transceiver (RMT) \- ESP32-S2 \- — ESP-IDF Programming Guide v5.1.3 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32s2/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v5.1.3/esp32s2/api-reference/peripherals/rmt.html)  
24. Enabling External PSRAM for Zephyr Applications on ESP32 SoCs · Developer Portal, accessed July 12, 2025, [https://developer.espressif.com/blog/2024/12/zephyr-how-to-use-psram/](https://developer.espressif.com/blog/2024/12/zephyr-how-to-use-psram/)  
25. ESP32 \- How To Use PSRAM \- ThingPulse, accessed July 12, 2025, [https://thingpulse.com/esp32-how-to-use-psram/](https://thingpulse.com/esp32-how-to-use-psram/)  
26. What's PSRAM for? : r/esp32 \- Reddit, accessed July 12, 2025, [https://www.reddit.com/r/esp32/comments/1fytrfn/whats\_psram\_for/](https://www.reddit.com/r/esp32/comments/1fytrfn/whats_psram_for/)  
27. Use PSRAM \- uPesy, accessed July 12, 2025, [https://www.upesy.com/blogs/tutorials/get-more-ram-on-esp32-with-psram](https://www.upesy.com/blogs/tutorials/get-more-ram-on-esp32-with-psram)  
28. How to allocate a vector or array in PSRAM · Issue \#273 · esp-rs/esp-idf-sys \- GitHub, accessed July 12, 2025, [https://github.com/esp-rs/esp-idf-sys/issues/273](https://github.com/esp-rs/esp-idf-sys/issues/273)  
29. esp-idf/docs/en/api-guides/external-ram.rst at master \- GitHub, accessed July 12, 2025, [https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/external-ram.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/external-ram.rst)  
30. How slow is PSRAM vs SRAM (anyone have quantitative info?) : r/esp32 \- Reddit, accessed July 12, 2025, [https://www.reddit.com/r/esp32/comments/ezs5sg/how\_slow\_is\_psram\_vs\_sram\_anyone\_have/](https://www.reddit.com/r/esp32/comments/ezs5sg/how_slow_is_psram_vs_sram_anyone_have/)  
31. PS Ram Speed \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=13356](https://esp32.com/viewtopic.php?t=13356)  
32. Teensy 4.1 PSRAM Random Access Latency, accessed July 12, 2025, [https://forum.pjrc.com/index.php?threads/teensy-4-1-psram-random-access-latency.62456/](https://forum.pjrc.com/index.php?threads/teensy-4-1-psram-random-access-latency.62456/)  
33. RMT Interrupt Delayed when WiFi STA Connect Failing \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=8064](https://esp32.com/viewtopic.php?t=8064)  
34. Support for External RAM \- ESP32 \- — ESP-IDF Programming Guide v5.0 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-guides/external-ram.html](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-guides/external-ram.html)  
35. Maximizing Execution Speed \- ESP32-S3 \- — ESP-IDF Programming Guide v5.0 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-guides/performance/speed.html](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-guides/performance/speed.html)  
36. Priority \- ESP32 Developer, accessed July 12, 2025, [https://esp32developer.com/programming-in-c-c/tasks/priority](https://esp32developer.com/programming-in-c-c/tasks/priority)  
37. The Async memcpy API \- ESP32-S2 \- — ESP-IDF Programming Guide v4.4.2 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s2/api-reference/system/async\_memcpy.html](https://docs.espressif.com/projects/esp-idf/en/v4.4.2/esp32s2/api-reference/system/async_memcpy.html)  
38. PSRAM \- \- — ESP-FAQ latest documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/storage/psram.html](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/storage/psram.html)  
39. Remote Control Transceiver (RMT) \- ESP32-H2 \- — ESP-IDF Programming Guide v5.1 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32h2/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32h2/api-reference/peripherals/rmt.html)  
40. Speed Optimization \- ESP32 \- — ESP-IDF Programming Guide v5.4.2 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/speed.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/speed.html)  
41. esp32.RMT \- GitHub Gist, accessed July 12, 2025, [https://gist.github.com/Andrei-Pozolotin/c4b3dd041efe53cfb92cfb4de9c67267](https://gist.github.com/Andrei-Pozolotin/c4b3dd041efe53cfb92cfb4de9c67267)  
42. Priority on task not high enough for time critical uart reading · Issue \#5049 · espressif/arduino-esp32 \- GitHub, accessed July 12, 2025, [https://github.com/espressif/arduino-esp32/issues/5049](https://github.com/espressif/arduino-esp32/issues/5049)