MEMORY
{
  PATCHSITE (rx) : ORIGIN = 0x08001320, LENGTH = 4
}

SECTIONS
{
  .text :
  {
    KEEP(*(.text.branch_to_hook))
  } > PATCHSITE
}
