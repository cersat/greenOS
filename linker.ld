ENTRY(_start)

SECTIONS
{
  . = 0x10000;

  .text : {
    *(.text.start)
    *(.text*)
    *(.rodata*)
  }

  .data : {
    *(.data*)
  }

  .bss : {
    PROVIDE(bss_start_marker = .);
    *(.bss*)
    *(COMMON)
    PROVIDE(bss_end_marker = .);
  }
}