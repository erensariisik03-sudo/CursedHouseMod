// Extract vehicle, wheel, trailer, attachment and model-placement code.
// Also emits call relationships and instruction listings for the selected functions.
// @category Custom

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.pcode.*;
import java.io.*;
import java.util.*;

public class ExtractVehicle extends GhidraScript {
    static final String[] KEYS = {
        "vehicle", "wheel", "trailer", "tractor", "attach", "implement",
        "tool", "chassis", "body", "model", "component", "suspension",
        "steer", "tire", "tyre", "trafficvehicle"
    };
    static final int MAX_FUNCTIONS = 2500;

    boolean match(String s) {
        if (s == null) return false;
        String l = s.toLowerCase();
        for (String k : KEYS) if (l.contains(k)) return true;
        return false;
    }
    void addFunction(Set<Function> set, Function f) {
        if (f != null && !f.isThunk()) set.add(f);
    }
    String safe(String s) { return s.replace('\n',' ').replace('\r',' '); }

    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args.length > 0 ? args[0] : "output");
        out.mkdirs();

        FunctionManager fm = currentProgram.getFunctionManager();
        Set<Function> targets = new LinkedHashSet<>();

        try (PrintWriter all = new PrintWriter(new File(out,"all_function_names.txt"))) {
            for (Function f : fm.getFunctions(true)) {
                all.println(f.getEntryPoint()+"\t"+f.getName(true));
                if (match(f.getName(true))) addFunction(targets,f);
            }
        }

        // Expand one call-graph hop in both directions around relevant functions.
        List<Function> seed = new ArrayList<>(targets);
        for (Function f : seed) {
            for (Address a : f.getBody().getAddresses(true)) {
                Instruction ins = currentProgram.getListing().getInstructionAt(a);
                if (ins == null) continue;
                for (Reference r : getReferencesFrom(a)) {
                    if (r.getReferenceType().isCall()) addFunction(targets, fm.getFunctionAt(r.getToAddress()));
                }
            }
            ReferenceIterator refs = getReferencesTo(f.getEntryPoint());
            while (refs.hasNext()) {
                Reference r = refs.next();
                addFunction(targets, fm.getFunctionContaining(r.getFromAddress()));
            }
        }

        DecompInterface decomp = new DecompInterface();
        decomp.toggleCCode(true);
        decomp.toggleSyntaxTree(true);
        decomp.openProgram(currentProgram);

        try (PrintWriter c = new PrintWriter(new BufferedWriter(new FileWriter(new File(out,"vehicle_decompiled.c"))));
             PrintWriter asm = new PrintWriter(new BufferedWriter(new FileWriter(new File(out,"vehicle_disassembly.txt"))));
             PrintWriter rel = new PrintWriter(new BufferedWriter(new FileWriter(new File(out,"vehicle_callgraph.txt"))));
             PrintWriter refs = new PrintWriter(new BufferedWriter(new FileWriter(new File(out,"vehicle_data_refs.txt"))))) {

            int n=0;
            for (Function f : targets) {
                if (monitor.isCancelled() || n >= MAX_FUNCTIONS) break;
                c.println("\n// ===== "+f.getName(true)+" @ "+f.getEntryPoint()+" =====");
                DecompileResults dr = decomp.decompileFunction(f,120,monitor);
                if (dr != null && dr.decompileCompleted() && dr.getDecompiledFunction()!=null)
                    c.println(dr.getDecompiledFunction().getC());
                else c.println("// DECOMPILER FAILED: "+(dr==null?"null":dr.getErrorMessage()));

                rel.println("\n["+f.getEntryPoint()+"] "+f.getName(true));
                asm.println("\n===== "+f.getName(true)+" @ "+f.getEntryPoint()+" =====");
                InstructionIterator ii=currentProgram.getListing().getInstructions(f.getBody(),true);
                while(ii.hasNext()) {
                    Instruction ins=ii.next();
                    asm.println(ins.getAddress()+"  "+ins);
                    for(Reference r:getReferencesFrom(ins.getAddress())) {
                        Function callee=fm.getFunctionAt(r.getToAddress());
                        if(callee!=null && r.getReferenceType().isCall())
                            rel.println("  CALL -> "+callee.getEntryPoint()+" "+callee.getName(true));
                        else if(r.getReferenceType().isData())
                            refs.println(f.getName(true)+" @ "+ins.getAddress()+" -> "+r.getToAddress()+" ["+r.getReferenceType()+"]");
                    }
                }
                n++;
            }
            println("Vehicle extraction complete: "+n+" functions");
        }
        decomp.dispose();

        // All defined strings containing vehicle/model-related hints.
        try(PrintWriter p=new PrintWriter(new File(out,"vehicle_strings.txt"))) {
            DataIterator it=currentProgram.getListing().getDefinedData(true);
            while(it.hasNext()) {
                Data d=it.next(); Object v=d.getValue();
                if(v instanceof String) {
                    String s=(String)v, l=s.toLowerCase();
                    if(match(l) || l.contains("wheel") || l.contains("position") ||
                       l.contains("rotation") || l.contains("translation") ||
                       l.contains("node") || l.contains("pivot") || l.contains("component")) {
                        p.println(d.getAddress()+"\t"+safe(s));
                        ReferenceIterator ri=getReferencesTo(d.getAddress());
                        while(ri.hasNext()) {
                            Reference r=ri.next();
                            Function f=fm.getFunctionContaining(r.getFromAddress());
                            if(f!=null) p.println("  REF "+f.getEntryPoint()+" "+f.getName(true));
                        }
                    }
                }
            }
        }
    }
}
