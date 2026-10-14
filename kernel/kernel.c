#include <stdint.h>
#include <stddef.h>

#define UART_BASE 0x10000000UL
#define UART_THR (*(volatile uint8_t *)(UART_BASE + 0))
#define UART_RBR (*(volatile uint8_t *)(UART_BASE + 0))
#define UART_LSR (*(volatile uint8_t *)(UART_BASE + 5))

#define SBI_EXT_TIME 0x54494D45UL
#define TIMER_INTERVAL 1000000UL

static volatile uint64_t ticks = 0;

static void uart_putc(char c)
{
    while ((UART_LSR & 0x20) == 0) {}
    UART_THR = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

static char uart_getc(void)
{
    while ((UART_LSR & 0x01) == 0)
        __asm__ volatile("nop");

    return (char)UART_RBR;
}

static uint64_t read_time(void)
{
    uint64_t value;
    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
}

static void sbi_set_timer(uint64_t deadline)
{
    register uint64_t a0 asm("a0") = deadline;
    register uint64_t a6 asm("a6") = 0;
    register uint64_t a7 asm("a7") = SBI_EXT_TIME;

    __asm__ volatile(
        "ecall"
        : "+r"(a0)
        : "r"(a6), "r"(a7)
        : "memory"
    );
}

static void timer_init(void)
{
    sbi_set_timer(read_time() + TIMER_INTERVAL);
    __asm__ volatile("csrs sie, %0" :: "r"(1UL << 5));
    __asm__ volatile("csrs sstatus, %0" :: "r"(1UL << 1));
}

static void print_number(uint64_t n)
{
    char buffer[24];
    int i = 0;

    if (n == 0) {
        uart_putc('0');
        return;
    }

    while (n > 0 && i < 24) {
        buffer[i++] = (char)('0' + n % 10);
        n /= 10;
    }

    while (i > 0)
        uart_putc(buffer[--i]);
}

/*
 * Educational system-call test.
 * These calls currently originate in supervisor mode,
 * not from a separate user-mode process.
 */
static uint64_t syscall_call(uint64_t number, uint64_t arg0)
{
    register uint64_t a0 asm("a0") = arg0;

    __asm__ volatile(
        "mv a7, %1\n\t"
        "ecall"
        : "+r"(a0)
        : "r"(number)
        : "a7", "memory"
    );

    return a0;
}

static int same_string(const char *a, const char *b)
{
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static void syscall_test(void)
{
    uart_puts("\n[SYSCALL] write test: ");

    syscall_call(1, 'O');
    syscall_call(1, 'K');

    uart_puts("\n[SYSCALL] getpid returned: ");
    print_number(syscall_call(2, 0));
    uart_puts("\n");
}

static void shell_help(void)
{
    uart_puts("\nNanoBoot Commands\n");
    uart_puts("-----------------\n");
    uart_puts("help    - Show commands\n");
    uart_puts("echo    - Print text\n");
    uart_puts("pid     - Show demo PID\n");
    uart_puts("ticks   - Show timer ticks\n");
    uart_puts("syscall - Test ecall dispatcher\n");
    uart_puts("uname   - Show kernel name\n");
    uart_puts("about   - Show project information\n");
    uart_puts("clear   - Clear terminal\n");
    uart_puts("reboot  - Stop QEMU\n");
}

static void shell_execute(char *line)
{
    while (*line == ' ')
        line++;

    if (*line == '\0')
        return;

    if (same_string(line, "help")) {
        shell_help();

    } else if (same_string(line, "syscall")) {
        syscall_test();

    } else if (same_string(line, "pid")) {
        uart_puts("Demo PID: 1\n");

    } else if (same_string(line, "ticks")) {
        uint64_t current = ticks;

        uart_puts("Timer ticks: ");
        print_number(current);

        uart_puts("\nElapsed time (approx.): ");
        print_number(current / 10);
        uart_puts(" seconds\n");

    } else if (same_string(line, "uname")) {
        uart_puts("NanoBoot RISC-V 64-bit kernel\n");

    } else if (same_string(line, "about")) {
        uart_puts("NanoBoot educational RISC-V mini-kernel\n");
        uart_puts("Platform: QEMU virt / OpenSBI / RV64\n");
        uart_puts("Features: UART shell, SBI timer, ecall demo\n");

    } else if (same_string(line, "clear")) {
        uart_puts("\033[2J\033[H");

    } else if (same_string(line, "reboot")) {
        uart_puts("Stopping QEMU. Goodbye!\n");
        *(volatile uint32_t *)0x00100000 = 0x5555;

        for (;;)
            __asm__ volatile("wfi");

    } else if (line[0] == 'e' &&
               line[1] == 'c' &&
               line[2] == 'h' &&
               line[3] == 'o' &&
               (line[4] == ' ' || line[4] == '\0')) {

        char *text = line + 4;

        while (*text == ' ')
            text++;

        uart_puts(text);
        uart_puts("\n");

    } else {
        uart_puts("Unknown command: ");
        uart_puts(line);
        uart_puts("\nType 'help' for available commands.\n");
    }
}

static void shell_run(void)
{
    char line[128];

    uart_puts("\nNanoBoot interactive shell\n");
    uart_puts("Type 'help' to see commands.\n");

    for (;;) {
        size_t length = 0;

        uart_puts("\nnanoboot> ");

        for (;;) {
            char c = uart_getc();

            if (c == '\r' || c == '\n') {
                uart_puts("\n");
                break;
            }

            if (c == '\b' || c == 127) {
                if (length > 0) {
                    length--;
                    uart_puts("\b \b");
                }
                continue;
            }

            if (c >= 32 && c <= 126 &&
                length < sizeof(line) - 1) {
                line[length++] = c;
                uart_putc(c);
            }
        }

        line[length] = '\0';
        shell_execute(line);
    }
}

/*
 * Called by trap_entry:
 * a0 = scause
 * a1 = saved a7 (system-call number)
 * a2 = saved a0 (first argument)
 * a3 = saved a1 (second argument)
 */
void trap_handler(uint64_t cause,
                   uint64_t number,
                   uint64_t arg0,
                   uint64_t arg1,
                   uint64_t *frame)
{
    (void)number;
    (void)arg0;
    (void)arg1;

    uint64_t interrupt = cause >> 63;
    uint64_t code = cause & 0x7fffffffffffffffUL;

    if (interrupt && code == 5) {
        ticks++;
        sbi_set_timer(read_time() + TIMER_INTERVAL);
        return;
    }

    if (interrupt)
        return;

    /* Environment call from supervisor mode. */
    if (code == 9) {
        uint64_t epc;
        uint64_t syscall_no = frame[15];
        uint64_t syscall_arg0 = frame[8];

        __asm__ volatile("csrr %0, sepc" : "=r"(epc));
        __asm__ volatile("csrw sepc, %0" :: "r"(epc + 4));

        if (syscall_no == 1) {
            uart_putc((char)syscall_arg0);
            frame[8] = 0;
            return;
        }

        if (syscall_no == 2) {
            frame[8] = 1;
            return;
        }

        uart_puts("\\n[SYSCALL] Unknown number: ");
        print_number(syscall_no);
        uart_puts("\\n");
        frame[8] = (uint64_t)-1;
        return;
    }

    uart_puts("\n[FATAL] Unexpected synchronous trap. scause=");
    print_number(code);
    uart_puts("\n");

    for (;;)
        __asm__ volatile("wfi");
}

void kernel_main(void)
{
    uart_puts("\n================================\n");
    uart_puts("       NanoBoot Mini-Kernel     \n");
    uart_puts("================================\n");
    uart_puts("[BOOT] Kernel started in S-mode\n");
    uart_puts("[UART] Console initialized\n");
    uart_puts("[TRAP] Trap vector installed\n");
    uart_puts("[TIMER] Initializing SBI timer...\n");

    timer_init();

    uart_puts("[TIMER] Timer interrupts enabled\n");
    uart_puts("[SYSCALL] Supervisor-mode ecall demo ready\n");

    shell_run();
}
