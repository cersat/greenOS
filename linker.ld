ENTRY(_start)

PHDRS
{
  text PT_LOAD FLAGS(5);  /* R+X: 0b101 */
  data PT_LOAD FLAGS(6);  /* R+W: 0b110 */
}

SECTIONS
{
  . = 0x10000;

  .text : {
    *(.text.start)
    *(.text*)
    *(.rodata*)
  } :text

  .data : {
    *(.data*)
  } :data

  .bss : {
    PROVIDE(bss_start_marker = .);
    *(.bss*)
    *(COMMON)
    PROVIDE(bss_end_marker = .);
  } :data
}
