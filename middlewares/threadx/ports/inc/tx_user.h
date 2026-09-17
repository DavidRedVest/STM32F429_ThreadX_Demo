#ifndef TX_USER_H_
#define TX_USER_H_

/* tx_initialize_low_level.S configures SysTick for a 1ms period (1kHz), not
 * the ThreadX default of 100 ticks/second (10ms) — override this to match so
 * any future "N * TX_TIMER_TICKS_PER_SECOND" style sleep/timeout expression
 * comes out in real seconds instead of being off by 10x. */
#define TX_TIMER_TICKS_PER_SECOND    1000UL

/* Without this, tx_thread_stack_highest_ptr is only ever set once at thread
 * creation and never updated (the update logic in tx_thread.h's
 * TX_THREAD_STACK_CHECK macro is compiled out), so app/src/app.c's
 * DispTaskInfo() would always print a near-zero, meaningless "MaxStack"
 * high-water-mark for every thread. This also enables ThreadX's built-in
 * stack-overflow detection (checks the TX_STACK_FILL guard pattern on
 * suspend/resume/relinquish). */
#define TX_ENABLE_STACK_CHECKING

#endif