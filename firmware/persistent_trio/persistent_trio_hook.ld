MEMORY
{
  PATCH (rx) : ORIGIN = 0x08005C00, LENGTH = 0x400
}

SECTIONS
{
  .text :
  {
    KEEP(*(.text.persistent_trio_hook))
    *(.text*)
    *(.rodata*)
  } > PATCH
}
