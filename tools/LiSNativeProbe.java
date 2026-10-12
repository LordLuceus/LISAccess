// Targeted static research only. Used with -noanalysis -readOnly -deleteProject.
// Addresses come from this build's native registration table, not a guessed SDK.
// @category LISAccess.Research
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

public class LiSNativeProbe extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2) throw new IllegalArgumentException("output then address:label pairs required");
        DecompInterface decompiler = new DecompInterface();
        try (PrintWriter output = new PrintWriter(Files.newBufferedWriter(Path.of(args[0]), StandardCharsets.UTF_8))) {
            if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException(decompiler.getLastMessage());
            output.println("Static, targeted decompilation. Research labels are derived from registration strings.");
            output.println("Executable: " + currentProgram.getExecutablePath());
            for (int i = 1; i < args.length; i++) {
                monitor.checkCancelled();
                // cmd.exe batch launchers split equals signs into arguments.
                String[] pair = args[i].split(":", 2);
                if (pair.length != 2) throw new IllegalArgumentException("Expected address:label");
                Address address = toAddr(pair[0]);
                disassemble(address);
                Function function = getFunctionAt(address);
                if (function == null) function = createFunction(address, "Research_" + pair[1]);
                if (function == null) throw new IllegalStateException("Could not create function at " + address);
                DecompileResults result = decompiler.decompileFunction(function, 30, monitor);
                output.println("\nFUNCTION " + pair[1] + " @ " + address);
                if (!result.decompileCompleted() || result.getDecompiledFunction() == null)
                    throw new IllegalStateException("Decompilation failed: " + result.getErrorMessage());
                output.println(result.getDecompiledFunction().getC());
                output.flush();
                println("PROBE_OK " + pair[1] + " " + address);
            }
        } finally { decompiler.dispose(); }
    }
}
