

# **A Comprehensive Architectural and Implementation Guide for an ESP32-Based Dual-Axis Motion Controller**

## **Section 1: Foundational Stepper Control with the RMT Peripheral**

The core of any high-performance motion control system is the ability to generate precise, timed pulse trains. For the ESP32 platform, the Remote Control Transceiver (RMT) peripheral is the ideal hardware for this task. Originally designed for infrared remote control protocols, its flexible architecture allows it to be repurposed as a highly accurate, multi-channel pulse generator, perfectly suited for driving stepper motors.1 This section establishes the foundational principles for leveraging the RMT peripheral, focusing on the modern, robust methodologies available in the ESP-IDF v5.x and later development frameworks.

### **1.1. The RMT Encoder: The Modern Approach to Pulse Generation**

The evolution of the ESP-IDF has seen a significant paradigm shift in how the RMT peripheral is programmed. Early versions of the framework required developers to manually construct arrays of rmt\_item32\_t structures, which directly represented the low-level pulse data stored in the RMT's memory.2 While direct, this approach was cumbersome, memory-inefficient for complex patterns, and exposed low-level hardware details like the RMT's internal "ping-pong" memory buffers to the application layer.3

The modern ESP-IDF (v5.0 and newer) introduces a superior abstraction: the RMT Encoder.5 This is not merely a syntactic change but a fundamental improvement in the driver architecture. An encoder is a stateful function responsible for converting high-level user data (e.g., a command to move 1000 steps at a certain speed) into the low-level

rmt\_symbol\_word\_t data that the RMT hardware understands, doing so on-the-fly as the RMT peripheral consumes data.5

This architectural choice carries profound implications. The encoder function is executed within the RMT's Interrupt Service Routine (ISR) context.8 This means it can continuously feed the RMT hardware with new pulse data without blocking the main application threads. However, it also means the encoder code must be highly efficient, non-blocking, and adhere to all ISR constraints (e.g., no use of standard mutexes, limited memory allocation). For maximum performance and to avoid issues with flash cache misses during other system operations (like Wi-Fi activity), encoder functions should be placed in IRAM using the

IRAM\_ATTR attribute.8

The RMT driver manages the interaction with the encoder. When a transmission is initiated via rmt\_transmit(), the driver calls the encoder function, providing it with a buffer in the RMT's dedicated hardware memory to fill with pulse data. If the entire command cannot be encoded at once (e.g., a very long sequence of steps), the encoder can signal RMT\_ENCODING\_MEM\_FULL. The driver will then transmit the prepared data and, once space is available, re-invoke the encoder to continue where it left off. The stateful nature of the encoder is paramount for this seamless, continuous operation.1

For a stepper motor controller, a custom "stepper pulse encoder" is the foundational software component. It will be designed to accept a simple payload, such as a struct containing the desired pulse timing and the number of pulses to generate. This encapsulates the core motor operation, forming a reusable building block for all higher-level motion commands. The official ESP-IDF provides an excellent example of such an encoder architecture in its stepper motor example, which serves as a valuable reference.10

### **1.2. Translating (step\_delay\_us, repeat\_count) into RMT Commands**

The user's core command tuple, (step\_delay\_us, repeat\_count), elegantly maps to the concepts of motor speed and travel distance. Translating this into the RMT domain requires careful configuration of both the RMT channel and the custom encoder.

#### **Speed (step\_delay\_us)**

The speed of a stepper motor is inversely proportional to the delay between steps. The step\_delay\_us parameter directly controls this period. To translate this microsecond value into hardware-level RMT timing, the RMT channel's clock source and resolution must be configured.

During channel initialization with rmt\_new\_tx\_channel(), the rmt\_tx\_channel\_config\_t structure is used. Its resolution\_hz member defines the frequency of the RMT's internal tick counter.6 Setting this to 1,000,000 Hz (1 MHz) is highly convenient, as it makes one RMT tick precisely equal to one microsecond.2

With a 1 MHz resolution, a standard stepper pulse can be generated using a single RMT symbol (rmt\_symbol\_word\_t). A typical step pulse requires a brief high period (e.g., 2-5 µs, as per the stepper driver's datasheet) followed by a low period. The step\_delay\_us defines the total period of one step. Therefore, the RMT symbol would be configured as:

* duration0: PULSE\_WIDTH\_US (e.g., 3\)  
* level0: 1 (High)  
* duration1: step\_delay\_us \- PULSE\_WIDTH\_US  
* level1: 0 (Low)

The custom stepper encoder will take step\_delay\_us as an input and use it to construct these symbols.

#### **Distance (repeat\_count)**

The repeat\_count parameter determines the total number of steps for a given movement command. There are two primary methods to implement this repetition.

1. **Hardware Looping (loop\_count):** The rmt\_transmit\_config\_t structure, passed to rmt\_transmit(), contains a loop\_count member.6 Setting this to a positive integer instructs the RMT hardware to automatically repeat the transmitted sequence that many times. While this is very CPU-efficient, it comes with a critical limitation: the entire sequence of RMT symbols to be looped must fit within a single RMT memory block (typically 48 or 64 symbols, depending on the ESP32 variant).11 For a stepper motor, where each step is one symbol, this limits hardware looping to very short movements. Furthermore, support for  
   loop\_count has been inconsistent across ESP-IDF versions and chip variants, with some configurations reporting it as unsupported.13 Given these constraints, hardware looping is not a suitable method for a flexible, general-purpose motion controller that needs to handle large  
   repeat\_count values.  
2. **Encoder-Level Looping (Recommended):** The superior and more flexible approach is to handle the repetition within the custom stepper encoder itself. The encoder must be designed to be stateful. Its state should include a counter initialized with the repeat\_count. Each time the RMT driver invokes the encoder function, it generates a batch of step pulse symbols, decrements its internal counter, and fills the provided hardware buffer. It continues this process across multiple invocations until its counter reaches zero, at which point it signals RMT\_ENCODING\_COMPLETE.1 This method is limited only by the data types used for the counter, not by the RMT's hardware memory size, and is the recommended architecture for this application.

### **1.3. Implementing Pauses ('Blank Steps')**

A pause, or a "blank step," is a command to do nothing for a specified duration. This is essential for timing-sensitive operations, such as allowing a tool to settle or for coordinated multi-axis movements. It is tempting to implement this by generating a single RMT pulse with a very long low-level duration. However, this is inefficient and problematic. The duration field in an RMT symbol is limited to 15 bits (32767 ticks), which at a 1 MHz resolution corresponds to a maximum pause of only \~32.7 milliseconds.

A much cleaner and more robust solution is to handle pauses at the command processing layer, outside of the RMT peripheral's direct involvement. The Lua script would generate a distinct "pause" command, which is then placed in the main command queue. The C-level motion control task (the RMT\_Feeder\_Task described in the next section) will be responsible for interpreting these commands.

When the dispatcher encounters a pause command (e.g., CMD\_PAUSE with a payload of duration\_ms), it will simply call the FreeRTOS delay function, vTaskDelay(pdMS\_TO\_TICKS(duration\_ms)). This blocks only the motion control task, allowing other system tasks to run, and cleanly separates the logic of pulse generation from the logic of timed waiting. This approach keeps the RMT driver and its encoders focused solely on their primary purpose: high-precision pulse generation.

## **Section 2: High-Throughput Command Processing with PSRAM**

A key requirement for this controller is a large command queue, enabling complex motion paths to be pre-loaded and executed. The ESP32's on-chip SRAM is limited (typically a few hundred kilobytes), making external PSRAM the only viable option for storing a queue of potentially thousands of commands.15 However, integrating PSRAM into a high-performance, real-time system introduces a significant architectural challenge: the RMT peripheral's DMA engine cannot directly access PSRAM. This section details the data pipeline architecture necessary to bridge this hardware gap efficiently.

### **2.1. The PSRAM-to-SRAM Bottleneck and the Double-Buffer Solution**

The ESP32 architecture has a fundamental limitation regarding DMA operations: DMA controllers, including the one integrated with the RMT peripheral, can only access data residing in the chip's internal SRAM.17 Any memory buffer that is to be used as a source for a DMA transfer must be allocated from this internal memory pool using the

MALLOC\_CAP\_DMA capability flag.18 PSRAM, being an external SPI-connected device, is outside this DMA-accessible address space.

This hardware constraint dictates a two-tiered memory architecture for the command pipeline. The application cannot simply point the rmt\_transmit function to a large command buffer in PSRAM. Instead, a mediating software layer is required to shuttle data from the slow, large PSRAM to the fast, small, DMA-accessible internal SRAM.

The most effective pattern to manage this is a producer-consumer model implemented with double-buffering (also known as ping-pong buffering). The architecture is as follows:

1. **PSRAM Command Queue (The Producer's Target):** A large buffer is allocated in PSRAM at startup using ps\_malloc() or heap\_caps\_malloc(size, MALLOC\_CAP\_SPIRAM).21 This buffer holds the "master" queue of motion commands generated by the Lua scripting layer.  
2. **Internal SRAM Double Buffers (The Consumer's Source):** Two smaller, identical buffers are allocated in internal, DMA-capable SRAM using heap\_caps\_malloc(size, MALLOC\_CAP\_DMA). These will be referred to as dma\_buffer\_A and dma\_buffer\_B.  
3. The RMT\_Feeder\_Task (The Consumer): A dedicated, high-priority FreeRTOS task is created to manage this data flow. Its sole responsibility is to:  
   a. Pull the next block of commands from the PSRAM queue.  
   b. Copy these commands into one of the internal SRAM buffers (e.g., dma\_buffer\_A).  
   c. Initiate an RMT transmission using dma\_buffer\_A.  
   d. While the RMT hardware is busy transmitting from dma\_buffer\_A, the task immediately begins pre-fetching the next block of commands from PSRAM and copies them into the other buffer, dma\_buffer\_B.  
   e. Once the transmission from dma\_buffer\_A is complete, it immediately starts the next transmission using dma\_buffer\_B, and the cycle repeats.

This double-buffering approach ensures that the RMT peripheral is never starved for data. While one buffer is being transmitted by the hardware, the CPU (or another DMA engine, as discussed next) is concurrently preparing the next buffer, effectively hiding the latency of the PSRAM-to-SRAM data copy.

### **2.2. Optimizing the Pipeline with esp\_async\_memcpy**

On ESP32 variants that support it (notably the ESP32-S3), the data transfer from PSRAM to the internal DMA buffers can be further optimized using the esp\_async\_memcpy API.23 This function utilizes a general-purpose DMA (GDMA) controller to perform memory copies asynchronously, offloading the CPU from this repetitive and time-consuming task. This is particularly beneficial as it frees the CPU within the

RMT\_Feeder\_Task to perform other logic, such as command parsing or status checking, while the data transfer happens in the background.

The implementation of this advanced pipeline involves a tightly coordinated dance between the feeder task, the async memcpy driver, and their respective callbacks:

1. **Initialization:** The async memcpy driver is installed once at startup via esp\_async\_memcpy\_install().  
2. **Initiation:** The RMT\_Feeder\_Task identifies the next command data in the PSRAM queue and the currently inactive internal DMA buffer. It then calls esp\_async\_memcpy to start the copy from the PSRAM source to the internal SRAM destination. Crucially, it provides a callback function and a signaling mechanism (like a FreeRTOS semaphore) as arguments.  
3. **Blocking and Yielding:** After initiating the async copy, the RMT\_Feeder\_Task blocks, waiting on the semaphore. This yields the CPU to other tasks.  
4. **ISR Callback:** The esp\_async\_memcpy function, upon completing the DMA transfer, invokes the provided callback. This callback is executed in a high-priority ISR context.23  
5. **Signaling:** Inside the ISR-safe callback, the code's primary job is to signal that the buffer is ready. It does this by giving the semaphore using the ISR-safe version of the function, xSemaphoreGiveFromISR. The callback must also return a value indicating whether a higher-priority task was unblocked, allowing the FreeRTOS scheduler to perform an immediate context switch if necessary.  
6. **Resumption:** The semaphore unblocks the RMT\_Feeder\_Task, which now knows that the internal DMA buffer is filled and ready. It can then proceed to pass this buffer to the rmt\_transmit function.

This interrupt-driven pipeline is exceptionally efficient, minimizing CPU involvement in the data movement and ensuring the RMT peripheral can be fed with new commands with very low latency. It is important to note that while esp\_async\_memcpy can be used to copy *from* PSRAM, the destination buffer for the RMT peripheral must still be in internal MALLOC\_CAP\_DMA memory.

### **2.3. PSRAM Performance and Cache Considerations**

While PSRAM provides a vast amount of memory, it is not a direct replacement for internal SRAM in terms of performance. PSRAM is an external SPI device, and its access latency and bandwidth are significantly lower than internal RAM.25 Performance is heavily influenced by the ESP32's cache architecture.

The ESP32 uses a cache to speed up access to external memories like flash and PSRAM. When data is read from PSRAM, it is brought into the cache. Subsequent reads of the same data are very fast, served directly from the cache. However, the cache is a finite size (e.g., 32 KB). Accessing large blocks of data or performing non-sequential, random-access reads across a wide memory range can lead to cache misses.27 A cache miss forces the CPU to stall while the data is fetched from the slow PSRAM, which can also evict previously cached program code from flash, leading to a double performance penalty: a delay for the data fetch and a subsequent delay when the evicted code needs to be re-fetched.15

Given that the command queue in this application is designed to be very large, the access pattern becomes critical. If the queue were implemented as a linked list, where each command node is allocated separately, the nodes could be scattered randomly throughout the 4 MB PSRAM address space. Traversing this list would result in a series of random memory accesses, leading to poor cache performance and high latency for the RMT\_Feeder\_Task.

Therefore, the recommended strategy is to structure the command queue in PSRAM as a single, large, contiguous circular buffer. This buffer should be allocated once at application startup. The Lua layer adds commands to the "tail" of the buffer, and the C-level RMT\_Feeder\_Task consumes them from the "head." This approach ensures that memory access is almost always sequential, which is highly cache-friendly. The CPU's pre-fetch mechanisms can work effectively, minimizing stalls and ensuring the data pipeline from PSRAM to the RMT peripheral remains as fast and deterministic as possible.

## **Section 3: Achieving Deterministic Timing and Synchronization**

For a dual-axis controller, executing individual movements with precision is only half the battle. The system must also handle transitions between movements—specifically, changing direction—with deterministic timing. Furthermore, it must be capable of starting movements on both axes simultaneously for coordinated motion like diagonal lines. This section details the advanced RMT techniques required to achieve these critical synchronization capabilities.

### **3.1. Precision Direction Control with the on\_trans\_done Callback**

A standard stepper motor driver has separate inputs for STEP and DIRECTION signals. To reverse a motor's direction, the DIRECTION pin's logic level must be toggled. Critically, most drivers specify a "setup time": the DIRECTION signal must be stable for a few microseconds before the first STEP pulse arrives to ensure the change is registered correctly.29 Simply changing the GPIO level in software and then immediately starting the next RMT transmission is unreliable, as software execution time is non-deterministic.

The ESP-IDF RMT driver provides the perfect mechanism for this: the on\_trans\_done callback. This callback is part of the rmt\_tx\_event\_callbacks\_t structure and is registered per-channel using rmt\_tx\_register\_event\_callbacks().8 It is an ISR function that the RMT driver executes the moment a transmission completes.

The implementation strategy for precise, back-to-back direction changes is as follows:

1. **Register Callback:** For each motor's RMT channel, register an on\_trans\_done callback function.  
2. **Pass Context:** Use the user\_data argument of the registration function to pass a pointer to a motor-specific context structure. This struct should contain essential information, such as the GPIO number of the direction pin.  
3. **Callback Logic:** Inside the ISR-safe callback, the first action is to read the *next* desired direction from a shared state (managed by the RMT\_Feeder\_Task) and immediately set the direction pin's level using gpio\_set\_level().  
4. **Signal Feeder Task:** After setting the direction, the callback signals the RMT\_Feeder\_Task (e.g., using a semaphore or task notification) that it is now safe to queue the next motion command.

However, this alone does not solve the setup time requirement. The on\_trans\_done callback fires *after* the last pulse of the previous command. The time it takes for the ISR to execute, the GPIO to change, the feeder task to wake up, and the next RMT transmission to be prepared and started is variable and can be very short.

To guarantee the driver's setup time, a more robust hardware-timed solution is needed. This can be achieved by prepending a "dummy" RMT symbol to the beginning of the *next* motion command's data. This symbol would be configured to generate a low-level signal (no pulse) for a duration equal to or greater than the required setup time (e.g., 5-10 µs). When the RMT\_Feeder\_Task prepares the next command after a direction change, it first encodes this delay symbol, then proceeds with the actual step pulse symbols. This ensures that after the direction pin is toggled in the callback, the RMT hardware itself enforces a precise, predictable delay before the first step pulse is issued, satisfying the driver's timing requirements reliably.

### **3.2. Synchronizing Dual-Motor Transmissions**

For true coordinated motion, such as moving a tool along a 45-degree path, both stepper motors must start their step pulse trains at the exact same moment. The ESP-IDF provides a dedicated feature for this: the RMT Sync Manager.

The Sync Manager is created using rmt\_new\_sync\_manager(), which takes a rmt\_sync\_manager\_config\_t structure. This structure simply contains an array of the RMT channel handles that are to be synchronized. Once a channel is part of a sync group, calling rmt\_transmit() on it will not start the transmission immediately. Instead, the channel enters a waiting state. The hardware will only begin transmitting on *all* channels in the group once rmt\_transmit() has been called for every single member of that group.

This works perfectly for a single, one-off synchronized move. The challenge arises when trying to execute a continuous sequence of back-to-back synchronized moves. Because the two commands (one for each motor) may have different numbers of steps, their transmissions will finish at different times. This breaks the synchronization for the next command pair. To re-establish it, the Sync Manager must be reset.

The standard function for this is rmt\_sync\_reset(). However, analysis and community reports have shown a critical flaw for high-performance applications: this function is **not ISR-safe** and introduces a significant delay, on the order of 100 microseconds, between transactions. This latency is unacceptable for smooth, continuous motion.

The expert solution is to bypass the high-level driver function and create a custom, ISR-safe version of the sync reset. This function directly manipulates the underlying hardware registers within an ISR-safe critical section. Based on community-developed workarounds, this function would look like this:

C

// NOTE: This is an advanced, low-level implementation.  
// 'synchro' is the rmt\_sync\_manager\_handle\_t.  
esp\_err\_t rmt\_sync\_reset\_isr(rmt\_sync\_manager\_handle\_t synchro) {  
    ESP\_RETURN\_ON\_FALSE(synchro, ESP\_ERR\_INVALID\_ARG, TAG, "invalid argument");  
    rmt\_group\_t\* group \= synchro-\>group;  
    portENTER\_CRITICAL\_ISR(\&group-\>spinlock);  
    // Reset the clock dividers for all channels in the sync mask  
    rmt\_ll\_tx\_reset\_channels\_clock\_div(group-\>hal.regs, synchro-\>channel\_mask);  
    // Reset the memory pointers for each channel in the sync group  
    for (size\_t i \= 0; i \< synchro-\>array\_size; i++) {  
        rmt\_ll\_tx\_reset\_pointer(group-\>hal.regs, synchro-\>tx\_channel\_array\[i\]-\>channel\_id);  
    }  
    portEXIT\_CRITICAL\_ISR(\&group-\>spinlock);  
    return ESP\_OK;  
}

This custom function, rmt\_sync\_reset\_isr, can be safely called from within an on\_trans\_done callback. The overall architecture for continuous synchronized motion becomes:

1. Create two RMT channels and add them to a Sync Manager.  
2. Register an on\_trans\_done callback for both channels.  
3. Use atomic flags or a shared counter to track when *both* channels have completed their respective transmissions.  
4. When the second channel finishes, its callback will execute rmt\_sync\_reset\_isr() to re-arm the synchronization.  
5. It will then signal the RMT\_Feeder\_Task to issue the next pair of synchronized rmt\_transmit() calls.

This architecture achieves low-latency, gapless, and continuous synchronized motion, overcoming the limitations of the standard driver.

## **Section 4: Failsafe Operation: Emergency Stop and Position Recovery**

In any motion control system, a robust emergency stop (E-stop) mechanism is a critical safety feature. However, simply halting the motors is not sufficient for a system that relies on absolute positioning. After an E-stop, the controller must know the *exact* position where the motors stopped. Without this information, the system is "lost" and requires a time-consuming re-homing procedure. This section details a comprehensive strategy for implementing an immediate, failsafe E-stop that allows for high-fidelity position recovery.

### **4.1. The Abort Mechanism: rmt\_disable()**

The correct and most reliable method to immediately halt an ongoing RMT transmission in modern ESP-IDF is the rmt\_disable() function.11 When called with a channel handle, this function performs several crucial actions:

* It immediately stops the RMT transmitter for that channel.  
* It disables the channel's interrupts.  
* It resets the peripheral's internal finite-state machine (FSM).  
* It implicitly calls the reset function for any associated encoder, clearing its internal state.8

In an E-stop event handler, rmt\_disable() should be called for both motor channels to ensure all pulse generation ceases instantly. It is important to note that older, legacy RMT driver functions like rmt\_tx\_stop() were known to be unreliable, particularly when looping or long transmissions were involved, and often failed to stop the transmission immediately or at all.30

rmt\_disable() is the modern, authoritative solution.

### **4.2. High-Fidelity Step Count Recovery**

The primary challenge with an E-stop is that when rmt\_disable() is called, the transmission is aborted mid-stream. Consequently, the on\_trans\_done callback, which normally provides the count of transmitted symbols, will not be triggered.11 The high-level driver API provides no other function to query the number of symbols that were successfully transmitted before the abort.32

To maintain absolute positioning, this count must be recovered. Two distinct strategies, with different trade-offs in precision and complexity, can be employed.

#### **Method A: Software-Based Transaction Chunking**

This approach prioritizes implementation simplicity over absolute precision. Instead of submitting a single large motion command (e.g., "move 10,000 steps") to the RMT driver, the RMT\_Feeder\_Task breaks the command into a series of smaller, sequential transactions (e.g., 100 transactions of 100 steps each).

* **Tracking:** A global, volatile, or atomic integer variable is used to track the number of *completed* transactions. This counter is incremented only within the on\_trans\_done callback, which fires after each small chunk is successfully transmitted.  
* **Recovery:** When an E-stop occurs, the controller calls rmt\_disable(). The last known position can be calculated as (completed\_chunks \* chunk\_size). The steps that were part of the in-flight, aborted chunk are lost.  
* **Precision:** The precision of this method is limited by the chunk size. A smaller chunk size (e.g., 10 steps) yields better precision but increases the software overhead due to more frequent transactions and ISR calls. A larger chunk size is more efficient but results in a larger potential position error after an E-stop.

#### **Method B: Hardware-Based Register Inspection (Recommended for Precision)**

For applications where exact position recovery is non-negotiable, a more advanced technique is required that queries the RMT hardware's state directly. The RMT peripheral maintains internal pointers to its memory block, indicating which rmt\_symbol\_word\_t it was processing at any given moment. By reading this pointer after an abort, the exact number of transmitted symbols can be calculated.

* **Implementation Steps:**  
  1. The E-stop handler first calls rmt\_disable() on the channel.  
  2. Immediately after, it calls rmt\_get\_channel\_status() (in older IDF versions, this required directly reading the RMT\_CHnSTATUS\_REG register).33  
  3. The returned status structure contains several fields, including the memory write/read offsets and the current state of the memory pointer. The key is to find the address within the RMT memory block that the channel was accessing when it was stopped.  
  4. The RMT\_Feeder\_Task knows the starting address of the DMA-capable internal SRAM buffer that it passed to the rmt\_transmit() call for the current transaction.  
  5. The number of transmitted symbols is calculated by finding the difference between the RMT's final memory pointer and the buffer's start address: symbols\_executed \= (rmt\_status.mem\_addr \- buffer\_start\_addr) / sizeof(rmt\_symbol\_word\_t).  
  6. Since the architecture uses one RMT symbol per stepper pulse, symbols\_executed is equal to the number of steps successfully sent before the abort.  
* **Precision:** This method provides to-the-pulse accuracy. The controller knows precisely how many steps were executed, allowing it to resume from the exact position after the E-stop condition is cleared. This method is more complex and relies on low-level hardware details that could be subject to change in future silicon or IDF revisions, but it offers unparalleled precision for maintaining absolute position integrity.

The following table summarizes the trade-offs between these two recovery methods.

| Feature | Method A: Software Chunking | Method B: Hardware Register Inspection |
| :---- | :---- | :---- |
| **Precision** | Limited by chunk size. Steps in the aborted chunk are lost. | To-the-pulse. Exact number of executed steps is recovered. |
| **Complexity** | Simple to implement. Relies only on high-level driver APIs. | Complex. Requires low-level register knowledge and pointer arithmetic. |
| **Overhead** | Higher software overhead due to frequent transaction submissions. | Minimal overhead. The check is only performed during an E-stop. |
| **Portability** | Highly portable across IDF versions. | Less portable. Relies on specific register layouts that may change. |
| **Recommendation** | Suitable for applications where a small, bounded position error after E-stop is acceptable. | **Recommended for high-precision applications** requiring absolute position integrity. |

## **Section 5: System Integration and Lua API Design**

With the low-level RMT control, data pipeline, and safety mechanisms defined, the final step is to integrate these components into a cohesive system and expose them to the high-level scripting layer. This involves creating a unified command architecture and designing a clean, powerful, and non-blocking Lua API.

### **5.1. Unified Command and Control Architecture**

The system must handle more than just motor movements; it needs to control lasers, read sensors, and perform other actions in a coordinated sequence. A unified command queue is the most effective way to manage this.

#### **Unified Command Queue**

The command queue, residing in PSRAM, should be designed to handle heterogeneous command types. This is best achieved with a C struct that uses a type-discriminating enum and a union for command-specific data:

C

typedef enum {  
    CMD\_MOVE,  
    CMD\_SYNC\_MOVE,  
    CMD\_PAUSE,  
    CMD\_LASER\_POWER,  
    CMD\_LIDAR\_READ\_REQUEST,  
    //... other commands  
} command\_type\_t;

// Example parameters for a single-axis move  
typedef struct {  
    uint8\_t axis;  
    uint32\_t step\_delay\_us;  
    uint32\_t repeat\_count;  
} move\_params\_t;

// Example parameters for a laser command  
typedef struct {  
    uint8\_t power\_percent;  
} laser\_params\_t;

// The unified command structure  
typedef struct {  
    command\_type\_t type;  
    uint32\_t command\_id; // Unique ID for status tracking  
    union {  
        move\_params\_t move;  
        //... other parameter structs  
        laser\_params\_t laser;  
        uint32\_t pause\_ms;  
    } params;  
} system\_command\_t;

#### **Central Command Dispatcher**

The RMT\_Feeder\_Task, previously responsible only for motor data, now evolves into a central command dispatcher. It runs in a continuous loop, dequeuing system\_command\_t items from the PSRAM queue and executing them based on their type:

* CMD\_MOVE / CMD\_SYNC\_MOVE: Prepare the data in the internal DMA buffers and initiate an RMT transmission as detailed in previous sections.  
* CMD\_PAUSE: Call vTaskDelay(pdMS\_TO\_TICKS(cmd.params.pause\_ms)).  
* CMD\_LASER\_POWER: Call a separate, dedicated function, e.g., laser\_control\_set\_power(cmd.params.laser.power\_percent).  
* CMD\_LIDAR\_READ\_REQUEST: Trigger a LIDAR sensor reading and, if the result is needed for subsequent commands, wait on a semaphore or event group that the LIDAR's data-ready callback will signal.

#### **Task Priorities and Core Affinity**

In a real-time system, careful management of FreeRTOS task priorities is non-negotiable. To guarantee smooth, stutter-free motion, the motion control tasks must not be preempted by lower-priority activities like handling Wi-Fi or processing Lua scripts.

* **High Priority:** The RMT\_Feeder\_Task and any other time-sensitive peripheral tasks (e.g., a LIDAR data processing task) should be assigned a high priority, just below the highest system-level tasks (like the Wi-Fi driver).35 RMT ISRs, by their nature, have the highest execution priority.  
* **Low Priority:** The task that runs the Lua interpreter, along with any web server or general application logic tasks, should be assigned a lower priority. The app\_main task, by default, runs at priority 1, which is a good baseline for non-critical activities.37  
* **Core Affinity:** To prevent resource contention, it is advisable to pin tasks to specific CPU cores using xTaskCreatePinnedToCore(). For instance, the entire motion control subsystem (RMT\_Feeder\_Task and RMT ISRs) could be pinned to Core 1, while the Lua interpreter, Wi-Fi stack, and other application logic run on Core 0\. This isolation prevents the Wi-Fi stack, which can cause interrupts and cache-intensive operations, from interfering with RMT timing.38

### **5.2. Crafting a High-Level Lua API**

The final piece of the architecture is the interface between the powerful C-level control system and the flexible Lua scripting environment. The goal is to create an API that is intuitive, powerful, and respects the asynchronous, non-blocking nature of the underlying system.

#### **Binding C to Lua**

Integrating C functions into Lua is a standard process. For each C function to be exposed, a corresponding Lua-compatible wrapper is created. This wrapper reads arguments from the Lua stack, calls the C function, and pushes any return values back onto the stack. These wrapper functions are then registered into the global Lua state using lua\_pushcfunction and lua\_setglobal.39 The

esp-idf-lua component provides the necessary Lua core library and build system integration to facilitate this.41

#### **API Design Principles**

The most important principle for this API is that it must be **non-blocking**. A Lua script should be able to queue up an entire, complex motion path consisting of thousands of commands in a few milliseconds and then yield control. It should not wait for the physical movements to complete. The C-level tasks are responsible for executing the queue in the background. The Lua script can then periodically poll the status of the queue or specific commands.

#### **Proposed Lua API Functions**

The following table outlines a proposed set of functions for the Lua API. These functions would be exposed to the Lua environment, for example, as motor.queue\_move(...).

| Lua Function Signature | Parameters | Description & C-Level Action |
| :---- | :---- | :---- |
| motor.queue\_move(axis, delay\_us, steps) | axis: 0 or 1\. delay\_us: Delay between steps. steps: Number of steps. | Creates a CMD\_MOVE command and enqueues it in the PSRAM command queue. Returns a unique command\_id for tracking. |
| motor.queue\_sync\_move(delay1, steps1, delay2, steps2) | Per-axis delay and step counts. | Creates a CMD\_SYNC\_MOVE command and enqueues it. Returns a unique command\_id. |
| motor.queue\_pause(duration\_ms) | duration\_ms: Pause duration in milliseconds. | Creates a CMD\_PAUSE command and enqueues it. Returns a command\_id. |
| motor.execute() | None | Signals the RMT\_Feeder\_Task (e.g., via a semaphore) to begin processing the commands currently in the queue. |
| motor.emergency\_stop() | None | Immediately calls the C-level E-stop handler, which invokes rmt\_disable() and performs position recovery. Clears the command queue. |
| motor.is\_busy() | None | Returns true if the command queue is not empty and the RMT\_Feeder\_Task is active. false otherwise. |
| motor.get\_position(axis) | axis: 0 or 1\. | Returns the current, known absolute position of the specified motor axis from the C-level state variables. |
| motor.get\_queue\_depth() | None | Returns the number of commands currently pending in the queue. |
| laser.set\_power(power) | power: 0-100. | Example of a non-motor command. Enqueues a CMD\_LASER\_POWER command. |

This API provides a powerful, flexible, and safe way for a user to define and control complex motion sequences from a high-level scripting language, while the low-level C and FreeRTOS implementation handles the demanding real-time execution and safety requirements.

## **Conclusion and Final Recommendations**

This report has outlined a comprehensive architecture for a high-performance, dual-axis stepper motor controller on the ESP32 platform. By leveraging the advanced features of the ESP-IDF, including the RMT peripheral, PSRAM, and FreeRTOS, it is possible to build a system that is both powerful and flexible, capable of executing complex, script-driven motion paths with high precision and safety.

### **Summary of Architecture**

The recommended architecture is a robust, three-tiered system that cleanly separates concerns:

1. **Lua Scripting Layer:** A high-level, user-facing interface for defining motion sequences. It provides flexibility and rapid development without requiring firmware recompilation for new motion paths.  
2. **PSRAM Command Queue:** A large, circular buffer in external PSRAM acts as the central message bus for the system. It decouples the scripting layer from the real-time core, allowing for the pre-loading of extensive and heterogeneous command lists.  
3. **Real-Time C/RTOS Core:** A set of high-priority, core-pinned FreeRTOS tasks and ISRs form the engine of the controller. This layer is responsible for the high-throughput data pipeline from PSRAM to the RMT, precise hardware-timed pulse generation, deterministic state changes, and robust safety and recovery mechanisms.

### **Key Recommendations Checklist**

For a successful implementation, the following key architectural and technical decisions are strongly recommended:

* **Embrace the Modern RMT Driver:** Utilize the ESP-IDF v5.x encoder-based RMT driver. Avoid legacy rmt\_item32\_t manipulation for a more abstract, maintainable, and powerful implementation.  
* **Implement a PSRAM-to-SRAM Pipeline:** Acknowledge the DMA-from-PSRAM hardware limitation and build a double-buffered data pipeline managed by a dedicated FreeRTOS task to feed the RMT peripherals.  
* **Optimize the Pipeline with esp\_async\_memcpy:** On supported chips (e.g., ESP32-S3), use the asynchronous memory copy feature to offload the CPU and create a highly efficient, interrupt-driven data path.  
* **Ensure Deterministic Timing:** Use the on\_trans\_done callback for critical actions like changing motor direction. Guarantee stepper driver setup times by prepending a short, hardware-timed delay pulse to subsequent motion commands.  
* **Use a Custom ISR-Safe Sync Reset:** For continuous, gapless synchronized motion, implement a custom, ISR-safe version of rmt\_sync\_reset that directly manipulates hardware registers to overcome the latency of the standard driver function.  
* **Prioritize Position Integrity:** Implement an emergency stop using rmt\_disable(). For absolute position recovery, use the hardware register inspection method to determine the exact number of steps executed before an abort.  
* **Design a Non-Blocking API:** The Lua API must be asynchronous. Functions should enqueue commands and return immediately, allowing the C core to handle execution in the background. This ensures a responsive and powerful scripting environment.  
* **Isolate Real-Time Tasks:** Use FreeRTOS task priorities and core affinity to shield the motion control subsystem from interference from non-critical tasks like Wi-Fi or other application logic, guaranteeing smooth and reliable motor operation.

#### **Works cited**

1. Remote Control Transceiver (RMT) \- ESP32 \- — ESP-IDF Programming Guide v5.4.2 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/rmt.html)  
2. ESP32: Sending short pulses with the RMT at Buildlog.Net Blog, accessed July 12, 2025, [https://www.buildlog.net/blog/2017/11/esp32-sending-short-pulses-with-the-rmt/](https://www.buildlog.net/blog/2017/11/esp32-sending-short-pulses-with-the-rmt/)  
3. ESP32 / RMT loosing ticks / \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=7365](https://esp32.com/viewtopic.php?t=7365)  
4. ESP32 / RMT loosing ticks, accessed July 12, 2025, [https://www.esp32.com/viewtopic.php?t=7365](https://www.esp32.com/viewtopic.php?t=7365)  
5. Peripherals \- ESP32 \- — ESP-IDF Programming Guide v5.1 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/migration-guides/release-5.x/5.0/peripherals.html](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/migration-guides/release-5.x/5.0/peripherals.html)  
6. Remote Control Transceiver (RMT) \- ESP32 \- — ESP-IDF Programming Guide v5.0 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-reference/peripherals/rmt.html)  
7. Peripherals \- ESP32 \- — ESP-IDF Programming Guide v5.0 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/migration-guides/release-5.x/peripherals.html](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/migration-guides/release-5.x/peripherals.html)  
8. Remote Control Transceiver (RMT) \- ESP32 \- — ESP-IDF Programming Guide v5.1 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-reference/peripherals/rmt.html)  
9. RMT Interrupt Delayed when WiFi STA Connect Failing \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=8064](https://esp32.com/viewtopic.php?t=8064)  
10. esp-idf/examples/peripherals/rmt/stepper\_motor/main/stepper\_motor\_example\_main.c at master \- GitHub, accessed July 12, 2025, [https://github.com/espressif/esp-idf/blob/master/examples/peripherals/rmt/stepper\_motor/main/stepper\_motor\_example\_main.c](https://github.com/espressif/esp-idf/blob/master/examples/peripherals/rmt/stepper_motor/main/stepper_motor_example_main.c)  
11. Remote Control Transceiver (RMT) \- ESP32 \- — ESP-IDF ..., accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32/api-reference/peripherals/rmt.html)  
12. ESP32 RMT LED Strip \- ESPHome, accessed July 12, 2025, [https://esphome.io/components/light/esp32\_rmt\_led\_strip.html](https://esphome.io/components/light/esp32_rmt_led_strip.html)  
13. Remote Control (RMT) \- ESP32-S2 \- — ESP-IDF Programming Guide v4.4 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s2/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s2/api-reference/peripherals/rmt.html)  
14. IDF 5.1+ and the new RMT API \- loop count is not supported \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=37533](https://esp32.com/viewtopic.php?t=37533)  
15. Support for External RAM \- ESP32 \- — ESP-IDF Programming Guide v5.4.2 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/external-ram.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/external-ram.html)  
16. esp-idf/docs/en/api-guides/external-ram.rst at master \- GitHub, accessed July 12, 2025, [https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/external-ram.rst](https://github.com/espressif/esp-idf/blob/master/docs/en/api-guides/external-ram.rst)  
17. Support for External RAM \- ESP32-S3 \- — ESP-IDF Programming Guide v5.4.2 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html)  
18. ESP32 I2S: With PSRAM present it should use MALLOC\_CAP\_DMA for DMA-based transfers · Issue \#429 · Makuna/NeoPixelBus \- GitHub, accessed July 12, 2025, [https://github.com/Makuna/NeoPixelBus/issues/429](https://github.com/Makuna/NeoPixelBus/issues/429)  
19. Support for external RAM — ESP-IDF Programming Guide v4.1-dev-2071-gf91080637 documentation \- Read the Docs, accessed July 12, 2025, [https://espressif-docs.readthedocs-hosted.com/projects/esp-idf/en/latest/api-guides/external-ram.html](https://espressif-docs.readthedocs-hosted.com/projects/esp-idf/en/latest/api-guides/external-ram.html)  
20. Heap Memory Allocation \- ESP32 \- — ESP-IDF Programming Guide v5.4.2 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/mem\_alloc.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/mem_alloc.html)  
21. ESP32 \- How To Use PSRAM \- ThingPulse, accessed July 12, 2025, [https://thingpulse.com/esp32-how-to-use-psram/](https://thingpulse.com/esp32-how-to-use-psram/)  
22. Use PSRAM \- uPesy, accessed July 12, 2025, [https://www.upesy.com/blogs/tutorials/get-more-ram-on-esp32-with-psram](https://www.upesy.com/blogs/tutorials/get-more-ram-on-esp32-with-psram)  
23. The Async memcpy API \- ESP32-S3 \- — ESP-IDF Programming ..., accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/system/async\_memcpy.html](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32s3/api-reference/system/async_memcpy.html)  
24. Asynchronous Memory Copy \- ESP32-C5 \- — ESP-IDF Programming Guide latest documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/system/async\_memcpy.html](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/system/async_memcpy.html)  
25. What's PSRAM for? : r/esp32 \- Reddit, accessed July 12, 2025, [https://www.reddit.com/r/esp32/comments/1fytrfn/whats\_psram\_for/](https://www.reddit.com/r/esp32/comments/1fytrfn/whats_psram_for/)  
26. How slow is PSRAM vs SRAM (anyone have quantitative info?) : r/esp32 \- Reddit, accessed July 12, 2025, [https://www.reddit.com/r/esp32/comments/ezs5sg/how\_slow\_is\_psram\_vs\_sram\_anyone\_have/](https://www.reddit.com/r/esp32/comments/ezs5sg/how_slow_is_psram_vs_sram_anyone_have/)  
27. PS Ram Speed \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=13356](https://esp32.com/viewtopic.php?t=13356)  
28. Support for External RAM \- ESP32 \- — ESP-IDF Programming Guide v5.0 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-guides/external-ram.html](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32/api-guides/external-ram.html)  
29. ESP32: Step Pulse Experiments with Timers at Buildlog.Net Blog, accessed July 12, 2025, [https://www.buildlog.net/blog/2017/11/esp32-step-pulse-experiments-with-timers/](https://www.buildlog.net/blog/2017/11/esp32-step-pulse-experiments-with-timers/)  
30. Remote peripheral ignores rmt\_tx\_stop command \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=3928](https://esp32.com/viewtopic.php?t=3928)  
31. \[TW\#17308\] Problem with RMT \- the rmt\_tx\_stop command does not work · Issue \#1456 · espressif/esp-idf \- GitHub, accessed July 12, 2025, [https://github.com/espressif/esp-idf/issues/1456](https://github.com/espressif/esp-idf/issues/1456)  
32. IDF 5.0 RMT peripheral features (IDFGH-8537) (IDFGH-8538) · Issue \#9991 · espressif/esp-idf \- GitHub, accessed July 12, 2025, [https://github.com/espressif/esp-idf/issues/9991](https://github.com/espressif/esp-idf/issues/9991)  
33. RMT \- ESP32 \- — ESP-IDF Programming Guide v4.3 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/peripherals/rmt.html](https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-reference/peripherals/rmt.html)  
34. RMT status register \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=4556](https://esp32.com/viewtopic.php?t=4556)  
35. Maximizing Execution Speed \- ESP32-S3 \- — ESP-IDF Programming Guide v5.0 documentation \- Espressif Systems, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-guides/performance/speed.html](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-guides/performance/speed.html)  
36. Speed Optimization \- ESP32 \- — ESP-IDF Programming Guide v5.4.2 documentation, accessed July 12, 2025, [https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/speed.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/performance/speed.html)  
37. ESP32 task priority with respect to main loop \- freertos \- Stack Overflow, accessed July 12, 2025, [https://stackoverflow.com/questions/77419368/esp32-task-priority-with-respect-to-main-loop](https://stackoverflow.com/questions/77419368/esp32-task-priority-with-respect-to-main-loop)  
38. IPC Interfering with RMT \- ESP32 Forum, accessed July 12, 2025, [https://esp32.com/viewtopic.php?t=24917](https://esp32.com/viewtopic.php?t=24917)  
39. Using Lua as ESP-IDF Component with ESP32 \- Espressif Developer Portal, accessed July 12, 2025, [https://developer.espressif.com/blog/using-lua-as-esp-idf-component-with-esp32/](https://developer.espressif.com/blog/using-lua-as-esp-idf-component-with-esp32/)  
40. Embedding Lua in your Projects \- Dror Gluska, accessed July 12, 2025, [https://blog.drorgluska.com/2022/11/embedding-lua-in-your-projects.html](https://blog.drorgluska.com/2022/11/embedding-lua-in-your-projects.html)  
41. georgik/esp-idf-component-lua \- GitHub, accessed July 12, 2025, [https://github.com/georgik/esp-idf-component-lua](https://github.com/georgik/esp-idf-component-lua)  
42. UncleRus/esp-idf-lua: Lua component for ESP-IDF \- GitHub, accessed July 12, 2025, [https://github.com/UncleRus/esp-idf-lua](https://github.com/UncleRus/esp-idf-lua)