
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.data.*;
import ghidra.program.model.mem.MemoryBlock;

import java.io.File;
import java.io.PrintWriter;
import java.util.*;

public class ExtractVehicle extends GhidraScript {

    private FunctionManager fm;
    private Listing listing;
    private Set<String> dumped = new HashSet<>();

    private void addFunction(Set<Function> set, Function f) {
        if (f != null) {
            set.add(f);
        }
    }

    private void dumpFunction(Function f, PrintWriter out) {
        if (f == null) return;

        String key = f.getEntryPoint().toString();
        if (dumped.contains(key)) return;
        dumped.add(key);

        out.println("\n\n==================================================");
        out.println("FUNCTION: " + f.getName(true));
        out.println("ADDRESS : " + f.getEntryPoint());
        out.println("SIGNATURE: " + f.getPrototypeString(true, true));
        out.println("==================================================");

        InstructionIterator instructions =
            listing.getInstructions(f.getBody(), true);

        while (instructions.hasNext()) {
            Instruction ins = instructions.next();
            out.println(ins.getAddress() + "  " + ins);
        }

        out.println("\n--- DECOMPILED ---");

        try {
            ghidra.app.decompiler.DecompInterface decompiler =
                new ghidra.app.decompiler.DecompInterface();

            decompiler.openProgram(currentProgram);

            ghidra.app.decompiler.DecompileResults result =
                decompiler.decompileFunction(f, 60, monitor);

            if (result.decompileCompleted() &&
                result.getDecompiledFunction() != null) {
                out.println(result.getDecompiledFunction().getC());
            } else {
                out.println("DECOMPILE FAILED: " + result.getErrorMessage());
            }

            decompiler.dispose();
        } catch (Exception e) {
            out.println("DECOMPILER ERROR: " + e);
        }
    }

    @Override
    public void run() throws Exception {

        fm = currentProgram.getFunctionManager();
        listing = currentProgram.getListing();

        File outputDir = new File(
    System.getProperty("user.dir"),
    "output/libfs14"
);
        outputDir.mkdirs();

        Set<Function> targets = new LinkedHashSet<>();

        String[] importantNames = {
            "Vehicle::load",
            "Wheel::load",
            "Wheel::updateGraphics",
            "Vehicle::update",
            "Vehicle::attachTool",
            "EntityManager::loadVehicle",
            "EntityManager::calculateVehicleSize"
        };

        for (Function f : fm.getFunctions(true)) {
            String name = f.getName(true);

            for (String target : importantNames) {
                if (name.contains(target)) {
                    targets.add(f);
                    break;
                }
            }
        }

        // Collect functions that reference important function entry points.
        for (Function f : new ArrayList<>(targets)) {
            Reference[] refs = getReferencesTo(f.getEntryPoint());

            for (Reference r : refs) {
                addFunction(
                    targets,
                    fm.getFunctionContaining(r.getFromAddress())
                );
            }
        }

        File decompiledFile =
            new File(outputDir, "vehicle_decompiled.c");

        try (PrintWriter out = new PrintWriter(decompiledFile)) {
            for (Function f : targets) {
                dumpFunction(f, out);
            }
        }

        // Call graph: functions called by the target functions.
        File callGraphFile =
            new File(outputDir, "vehicle_callgraph.txt");

        try (PrintWriter out = new PrintWriter(callGraphFile)) {
            for (Function f : targets) {
                out.println("\nFUNCTION " + f.getName(true) +
                            " @ " + f.getEntryPoint());

                InstructionIterator it =
                    listing.getInstructions(f.getBody(), true);

                while (it.hasNext()) {
                    Instruction ins = it.next();

                    for (Reference ref : ins.getReferencesFrom()) {
                        Function called =
                            fm.getFunctionAt(ref.getToAddress());

                        if (called != null) {
                            out.println("  CALL " +
                                called.getEntryPoint() + " " +
                                called.getName(true));
                        }
                    }
                }
            }
        }

        // Data references to strings and data objects.
        File dataRefsFile =
            new File(outputDir, "vehicle_data_refs.txt");

        try (PrintWriter out = new PrintWriter(dataRefsFile)) {
            for (Data d : listing.getDefinedData(true)) {
                if (monitor.isCancelled()) break;

                String type = d.getDataType().getName();
                String value = "";

                try {
                    value = String.valueOf(d.getValue());
                } catch (Exception ignored) {}

                if (type.contains("string") ||
                    value.toLowerCase().contains("wheel") ||
                    value.toLowerCase().contains("vehicle") ||
                    value.toLowerCase().contains("tire") ||
                    value.toLowerCase().contains("chassis") ||
                    value.toLowerCase().contains("cab")) {

                    out.println("\nDATA " + d.getAddress() +
                                " TYPE " + type +
                                " VALUE " + value);

                    Reference[] refs = getReferencesTo(d.getAddress());

                    for (Reference r : refs) {
                        Function f =
                            fm.getFunctionContaining(r.getFromAddress());

                        if (f != null) {
                            out.println("  REF " +
                                f.getEntryPoint() + " " +
                                f.getName(true));
                        }
                    }
                }
            }
        }

        // All function names for further investigation.
        File functionNamesFile =
            new File(outputDir, "all_function_names.txt");

        try (PrintWriter out = new PrintWriter(functionNamesFile)) {
            for (Function f : fm.getFunctions(true)) {
                out.println(f.getEntryPoint() + " " + f.getName(true));
            }
        }

        println("Vehicle extraction completed.");
        println("Output directory: /output");
    }
}
