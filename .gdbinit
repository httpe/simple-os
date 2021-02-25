python
# Usage: gdb) add-symbol-file-auto xxx.elf
class AddSymbolFileAuto (gdb.Command):
    def __init__(self):
        super(AddSymbolFileAuto, self).__init__("add-symbol-file-auto", gdb.COMMAND_USER)

    def invoke(self, solibpath, from_tty):
        offset = self.get_text_offset(solibpath)
        gdb_cmd = "add-symbol-file %s %s" % (solibpath, offset)
        gdb.execute(gdb_cmd, from_tty)

    def get_text_offset(self, solibpath):
        import subprocess
        # Note: Replace "readelf" with path to binary if it is not in your PATH.
        elfres = subprocess.check_output(["readelf", "-WS", solibpath])
        for line in elfres.decode('UTF-8').splitlines():
            if "] .text " in line:
                
                return "0x" + line.split()[4]
        return ""  # TODO: Raise error when offset is not found?
AddSymbolFileAuto()
end
