### SysConfig Notice

All examples will soon be supported by SysConfig, a tool that will help you graphically configure your software components. A preview is available today in the examples/syscfg_preview directory. Starting in 3Q 2019, with SDK version 3.30, only SysConfig-enabled versions of examples will be provided. For more information, click [here](http://www.ti.com/sysconfignotice).

---
# i2secho

---

## Example Summary

Example that uses the I2S driver to echo back to the audio received from the
Line In (Audio source) over the Line Out(headphones|speakers).
This example shows:
 * how to initialize the I2S driver in streaming mode with audio echo.
 * how to achieve I2S transfers with CD quality (16bits - 44.1 kHz)
 * how to treat (filter) sample data

## Peripherals Exercised

* `Board_GPIO_LED0` - Indicates that the board was initialized within
`mainThread()`
* `Board_I2C0` - Used to configure the TI codec on the Audio BoosterPack
* `Board_I2S0` - Used to echo sounds received from Audio In on Audio Out

## Resources & Jumper Settings

> If you're using an IDE (such as CCS or IAR), please refer to Board.html in
your project directory for resources used and board-specific jumper settings.
Otherwise, you can find Board.html in the directory
&lt;SDK_INSTALL_DIR&gt;/source/ti/boards/&lt;BOARD&gt;.

* This example was designed to use the CC3200 Audio BoosterPack
(CC3200AUDBOOST). The BoosterPack is required to successfully complete this
example.

* A pair of (headphones|speakers) and an audio source (smartphone) are required to observe functionality.

## Example Usage

* Mount the CC3200 Audio BoosterPack.

* Connect headphones or speakers to the Audio BoosterPack and the audio source.

* Run the example. `Board_GPIO_LED0` turns ON to indicate driver
initialization is complete.

* Sounds captured audio line in are echoed after basic filtering.

* The quality of the audio output is directly related to the quality of your audio
source. For this purpose, avoid using a highly disturbed audio source.
For example, when using your smartphone as an audio source, avoid leaving it in charge
at the same time: this creates a lot of audio disturbances.

> If you halt the target during execution of this example, and then
run again, the echoing will not resume. You will need to reload the program.

## Application Design Details

* One thread, `echoThread` is used to configure the codec and start
the I2S transfers.

* Ten I2S_Transactions are declared and a buffer is associate to each
of them (in this example, the buffer hold by a transaction remains the
same during all the execution).

* The I2S transactions are initially queued in two lists: `i2sReadList`
and `i2sWriteList`. `treatmentList` will be populated latter (i.e. when
some transaction of the `i2sReadList` will be finished).

* The buffers hold by the transactions are successively written (by the
read interface), treated and send out (by the write interface).
Here is a scheme showing the path followed by a transaction:

    `i2sReadList`          `treatmentList`          `i2sWriteList`
    [Transaction] ... (1)   [Transaction] ... (2)   [Transaction]....  (3)
    [Transaction]   :......>[Transaction]   :       [Transaction]   :
    [Transaction]                           :       [Transaction]   :
 ..>[Transaction]                           :......>[Transaction]   :
 :                                                                  :
 :..................................................................:

*(1) The read interface systematically fills the buffer contained in the head
transaction of the i2sReadList. When the buffer hold by this transaction is
full, the transaction is dequeued from the i2sReadList  and queued as the
tail of the treatmentList. The read interface continues receiving data by
using the next transaction of the i2sReadList.*

*(2) The treatment function treats the sample data contained by the transactions
queued in the treatmentList. The transaction treated is the head transaction
of the treatmentList. Once this transaction is treated, the transaction is
dequeued from the treatmentList and queued in the i2sWriteList.*

*(3) The write interface systematically sends out the buffer contained in the
head transaction of the i2sWriteList. When the buffer hold by this transaction
has been completely sent out, the corresponding transaction is dequeued from
the i2sWriteList and queued in the i2sReadList . The write interface continues
sending out data by using the next transaction of the i2sWriteList.*

> The drivers by default are non-instrumented in order to limit code size.

TI-RTOS:

* When building in Code Composer Studio, the kernel configuration project will
be imported along with the example. The kernel configuration project is
referenced by the example, so it will be built first. The "release" kernel
configuration is the default project used. It has many debug features disabled.
These feature include assert checking, logging and runtime stack checks. For a
detailed difference between the "release" and "debug" kernel configurations and
how to switch between them, please refer to the SimpleLink MCU SDK User's
Guide. The "release" and "debug" kernel configuration projects can be found
under &lt;SDK_INSTALL_DIR&gt;/kernel/tirtos/builds/&lt;BOARD&gt;/(release|debug)/(ccs|gcc).

FreeRTOS:

* Please view the `FreeRTOSConfig.h` header file for example configuration
information.
