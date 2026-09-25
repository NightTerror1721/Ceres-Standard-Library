# `<ceres/dma.h>`

DMA controller (0xFF040000): copies memory to memory without the program moving it word by word. See CeresASM docs/07-IO-Devices-and-Ports.md.

Source and destination are PHYSICAL addresses (translate them first if paging is on), and the two ranges must not overlap. A range that runs past the end of RAM is clamped: the copy moves what fits and dma_transferred() says how much. Arming a transfer clears DONE and sets BUSY; the copy lands on the controller's next tick (one instruction later) and raises interrupt 18 (IRQ_DMA), which a program may attach a handler to (ceres/irq.h) instead of polling.

Worth using from a few hundred bytes up: for less, a memcpy loop is as fast and never waits.

```c
#define DMA_SRC         (DMA_BASE + 0x00)   // W: source address
#define DMA_DST         (DMA_BASE + 0x04)   // W: destination address
#define DMA_LEN         (DMA_BASE + 0x08)   // W: bytes
#define DMA_CMD         (DMA_BASE + 0x0C)   // W: 1 arms the transfer
#define DMA_STATUS      (DMA_BASE + 0x10)   // R: bit0 BUSY, bit1 DONE
#define DMA_TRANSFERRED (DMA_BASE + 0x14)   // R: bytes the last completed transfer moved

#define DMA_CMD_START   1
#define DMA_BUSY        1
#define DMA_DONE        2

unsigned int dma_copy(void* dst, const void* src, unsigned int n);   // waits; returns the bytes moved
void         dma_copy_async(void* dst, const void* src, unsigned int n);   // returns at once
int          dma_busy(void);            // a transfer is in flight
int          dma_done(void);            // the last transfer has completed
unsigned int dma_transferred(void);     // bytes the last completed transfer moved
void         dma_wait(void);            // halts until the transfer in flight is done
```
