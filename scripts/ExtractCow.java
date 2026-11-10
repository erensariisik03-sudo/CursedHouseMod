// Ghidra headless script: inek / sut / gubre / balya / ahir ile ilgili
// sembolleri, string'leri ve ilgili fonksiyonlarin decompile ciktisini cikarir.
// @category Custom

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class ExtractCow extends GhidraScript {

    static final String[] KEYS = {
        "cow", "milk", "manure", "slurry", "dung", "barn", "stable", "animal",
        "bale", "baler", "straw", "hay", "silage", "feed", "fodder", "trough",
        "pasture", "husbandry", "livestock", "calf", "bull", "udder",
        "inek", "sut", "gubre", "ahir", "balya"
    };
    static final int MAX_DECOMPILE = 500;

    boolean match(String s) {
        if (s == null) return false;
        String l = s.toLowerCase();
        for (String k : KEYS) if (l.contains(k)) return true;
        return false;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        String outDir = args.length > 0 ? args[0] : "output";
        new File(outDir).mkdirs();

        Set<Function> targets = new LinkedHashSet<>();

        // 1) Fonksiyon adlari
        try (PrintWriter all = new PrintWriter(new File(outDir, "all_function_names.txt"))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                all.println(f.getEntryPoint() + "\t" + f.getName(true));
                if (match(f.getName(true))) targets.add(f);
            }
        }

        // 2) Eslesen semboller
        try (PrintWriter sym = new PrintWriter(new File(outDir, "symbols_matched.txt"))) {
            SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
            while (it.hasNext()) {
                Symbol s = it.next();
                if (match(s.getName(true))) {
                    sym.println(s.getAddress() + "\t" + s.getSymbolType() + "\t" + s.getName(true));
                }
            }
        }

        // 3) Eslesen string'ler ve onlara referans veren fonksiyonlar
        try (PrintWriter str = new PrintWriter(new File(outDir, "strings_matched.txt"))) {
            DataIterator di = currentProgram.getListing().getDefinedData(true);
            while (di.hasNext() && !monitor.isCancelled()) {
                Data d = di.next();
                Object v = d.getValue();
                if (!(v instanceof String)) continue;
                String s = (String) v;
                if (!match(s)) continue;
                StringBuilder refs = new StringBuilder();
                for (Reference r : getReferencesTo(d.getAddress())) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    if (f != null) {
                        targets.add(f);
                        refs.append(f.getName(true)).append(" ");
                    }
                }
                str.println(d.getAddress() + "\t\"" + s.replace("\n", "\\n") + "\"\t-> " + refs);
            }
        }

        // 4) Decompile
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        int n = 0;
        try (PrintWriter c = new PrintWriter(new File(outDir, "decompiled_functions.c"))) {
            for (Function f : targets) {
                if (monitor.isCancelled() || n >= MAX_DECOMPILE) break;
                DecompileResults r = ifc.decompileFunction(f, 60, monitor);
                c.println("// ===== " + f.getName(true) + " @ " + f.getEntryPoint() + " =====");
                if (r != null && r.decompileCompleted() && r.getDecompiledFunction() != null) {
                    c.println(r.getDecompiledFunction().getC());
                } else {
                    c.println("// decompile basarisiz");
                }
                n++;
            }
        }
        ifc.dispose();
        println("Bitti: " + targets.size() + " hedef fonksiyon, " + n + " tanesi decompile edildi.");
    }
}
