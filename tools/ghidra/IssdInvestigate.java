// Focused ROM evidence export. Run with analyzeHeadless -noanalysis.
// @category ISSD
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.SourceType;
import java.io.*;
import java.math.BigInteger;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

public class IssdInvestigate extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 3) throw new IllegalArgumentException("asm-file output-file start:end [...]");
        Map<Long, String> labels = new TreeMap<>();
        Pattern pattern = Pattern.compile("^(CODE|ADDR|DATA)_([0-9A-F]{6}):.*");
        for (String line : Files.readAllLines(Paths.get(args[0]), java.nio.charset.StandardCharsets.ISO_8859_1)) {
            Matcher match = pattern.matcher(line);
            if (match.matches()) labels.put(Long.parseLong(match.group(2), 16), match.group(1) + "_" + match.group(2));
        }
        try (PrintWriter out = new PrintWriter(args[1], "UTF-8")) {
            out.println("Program: " + currentProgram.getName());
            out.println("Language: " + currentProgram.getLanguageID());
            out.println("ROM SHA256: " + currentProgram.getExecutableSHA256());
            out.println("Focused linear decoding; M=X=E=0 at entry unless m1x1 specified; REP/SEP propagate.");
            out.println("Labels supplied by existing ISSD disassembly. No whole-ROM analysis or inferred call graph.");
            for (int n = 2; n < args.length; n++) {
                String[] range = args[n].split(":");
                long first = Long.parseLong(range[0], 16), last = Long.parseLong(range[1], 16);
                Address start = toAddr(first), end = toAddr(last);
                out.println("\n=== " + args[n] + " ===");
                for (Map.Entry<Long,String> entry : labels.entrySet()) {
                    if (entry.getKey() >= first && entry.getKey() <= last)
                        createLabel(toAddr(entry.getKey()), entry.getValue(), true, SourceType.USER_DEFINED);
                }
                if (range.length > 2 && range[2].equals("data")) {
                    for (long p = first; p <= last; p += 16) {
                        int size = (int)Math.min(16, last - p + 1);
                        byte[] bytes = getBytes(toAddr(p), size);
                        out.printf("%06X %s%n", p, HexFormat.of().formatHex(bytes));
                    }
                    continue;
                }
                clearListing(start, end);
                for (String register : new String[]{"ctx_MF", "ctx_XF", "ctx_EF"}) {
                    boolean narrow = range.length > 2 && range[2].equals("m1x1") && !register.equals("ctx_EF");
                    currentProgram.getProgramContext().setValue(currentProgram.getRegister(register), start, end, narrow ? BigInteger.ONE : BigInteger.ZERO);
                }
                Address cursor = start;
                while (cursor.compareTo(end) <= 0 && !monitor.isCancelled()) {
                    DisassembleCommand command = new DisassembleCommand(cursor, new AddressSet(start, end), false);
                    command.applyTo(currentProgram, monitor);
                    Instruction instruction = getInstructionAt(cursor);
                    if (instruction == null) { out.println(cursor + " UNDECODED"); break; }
                    out.printf("%06X %-12s %s%n", cursor.getOffset(), HexFormat.of().formatHex(instruction.getBytes()), instruction);
                    cursor = cursor.add(instruction.getLength());
                }
                if (monitor.isCancelled()) throw new IOException("Investigation cancelled");
            }
            out.println("EXPORT_COMPLETE");
            if (out.checkError()) throw new IOException("Cannot write evidence output");
        }
        println("ISSD evidence written: " + args[1]);
    }
}
