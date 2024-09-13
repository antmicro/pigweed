"""Bazel rules for creating fastboot binaries."""

load("@pigweed//pw_build:binary_tools.bzl", "pw_elf_to_bin", "pw_pad_binary")

def pw_fastboot_binary(name, executable, **kwargs):
    """Generate a basic fastboot binary from an executable.

    Convert an executable to a flat binary using objcopy and append
    required padding to ensure the binary is aligned to the block
    size (4096) used by the fastboot CLI.

    kwargs are passed directly to the pw_elf_to_bin rule used for
    elf to bin conversion, which can be used to remove sections
    that should not be present in the final binary.

    Args:
      name: Output file name of the fastboot binary.
      executable: Executable to generate the binary from.
      **kwargs: Passed to pw_elf_to_bin.
    """
    if type(executable) != "string":
        fail(
            "The 'executable' attribute must be a single label, " +
            "got {} of type {}".format(executable, type(executable)),
        )

    tempdir = "{name}.temp".format(name = name)
    elf2bin_file = "{tempdir}/_elf2bin".format(tempdir = tempdir)

    pw_elf_to_bin(
        name = name + ".elf2bin",
        elf_input = executable,
        bin_out = elf2bin_file,
        **kwargs
    )

    pw_pad_binary(
        name = name + ".pad_binary",
        bin_input = elf2bin_file,
        bin_out = name,
        pad_to_alignment = 4096,
        pad_byte = 0xFF,
    )
