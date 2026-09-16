package com.issdnative;

import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/**
 * Android entry point.
 *
 * SDLActivity does the heavy lifting - window, GL surface, input, audio and
 * the native thread that calls SDL_main. This subclass adds what a
 * handheld/phone build needs: the libraries to load, immersive fullscreen,
 * writable storage setup (including mods directory), and a Storage Access
 * Framework (SAF) file picker fallback if no cartridge ROM is found.
 */
public class ISSDActivity extends SDLActivity {

    private static final String TAG = "ISSDActivity";
    private static final int REQUEST_PICK_ROM = 1001;
    private static final int REQUEST_PICK_MODS_FOLDER = 1002;
    private static final String PREFS = "issd_android";
    private static final String PREF_MODS_TREE_URI = "mods_tree_uri";

    static {
        System.loadLibrary("SDL2");
        System.loadLibrary("main");
    }

    @Override
    protected String[] getLibraries() {
        // Order matters: libmain.so links against libSDL2.so.
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setTheme(R.style.Theme_ISSD);
        File ext = getExternalFilesDir(null);
        File targetDir = ext != null ? ext : getFilesDir();

        File modsDir = new File(targetDir, "mods");
        if (!modsDir.exists()) {
            modsDir.mkdirs();
        }
        syncConfiguredModsFolder(modsDir);

        nativeSetPaths(
            targetDir.getAbsolutePath(),
            getFilesDir().getAbsolutePath());

        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        // Check if any .sfc or .smc ROM exists in target directory
        if (!hasRomFile(targetDir)) {
            Log.i(TAG, "No ROM detected in " + targetDir.getAbsolutePath() + ", launching file picker");
            promptPickRom();
        }
    }

    public void requestRomPickerFromNative() {
        runOnUiThread(this::promptPickRom);
    }

    public void requestModsFolderPickerFromNative() {
        runOnUiThread(this::promptPickModsFolder);
    }

    private boolean hasRomFile(File dir) {
        if (dir == null || !dir.exists()) return false;
        File[] files = dir.listFiles();
        if (files == null) return false;
        for (File f : files) {
            if (!f.isDirectory()) {
                String name = f.getName().toLowerCase();
                if (name.endsWith(".sfc") || name.endsWith(".smc")) {
                    return true;
                }
            }
        }
        return false;
    }

    private void promptPickRom() {
        Toast.makeText(this, "Please select an ISS Deluxe (.sfc / .smc) ROM", Toast.LENGTH_LONG).show();
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        String[] mimeTypes = {"application/octet-stream", "*/*"};
        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
        try {
            startActivityForResult(intent, REQUEST_PICK_ROM);
        } catch (Exception e) {
            Log.e(TAG, "Failed to launch ACTION_OPEN_DOCUMENT", e);
        }
    }

    private void promptPickModsFolder() {
        Toast.makeText(this, "Select the folder that contains your ISSD mod packs", Toast.LENGTH_LONG).show();
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
            | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
            | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        try {
            startActivityForResult(intent, REQUEST_PICK_MODS_FOLDER);
        } catch (Exception e) {
            Log.e(TAG, "Failed to launch ACTION_OPEN_DOCUMENT_TREE", e);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_PICK_ROM && resultCode == RESULT_OK && data != null) {
            Uri uri = data.getData();
            if (uri != null) {
                copyRomFromUri(uri);
            }
        } else if (requestCode == REQUEST_PICK_MODS_FOLDER && resultCode == RESULT_OK && data != null) {
            Uri uri = data.getData();
            if (uri != null) {
                rememberAndSyncModsFolder(uri, data.getFlags());
            }
        }
    }

    private void copyRomFromUri(Uri uri) {
        Toast.makeText(this, "Installing ROM...", Toast.LENGTH_SHORT).show();
        new Thread(() -> importRomFromUri(uri), "ISSD-ROM-Import").start();
    }

    private void importRomFromUri(Uri uri) {
        File ext = getExternalFilesDir(null);
        File targetDir = ext != null ? ext : getFilesDir();
        File dstFile = new File(targetDir, "isss_deluxe.sfc");
        File tmpFile = new File(targetDir, "isss_deluxe.sfc.importing");

        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(tmpFile)) {
            if (in == null) {
                runOnUiThread(() -> Toast.makeText(this, "Failed to open selected file", Toast.LENGTH_SHORT).show());
                return;
            }
            byte[] buf = new byte[65536];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
            out.flush();
            if (tmpFile.length() == 0) {
                throw new IllegalStateException("selected ROM was empty");
            }
            replaceFile(tmpFile, dstFile);
            Log.i(TAG, "ROM copied successfully to " + dstFile.getAbsolutePath() + " (" + dstFile.length() + " bytes)");

            runOnUiThread(() -> {
                Toast.makeText(this, "ROM installed! Restarting...", Toast.LENGTH_SHORT).show();
                recreate();
            });
        } catch (Exception e) {
            deleteRecursively(tmpFile);
            Log.e(TAG, "Failed to copy ROM from URI: " + uri, e);
            runOnUiThread(() -> Toast.makeText(this, "Failed to copy ROM: " + e.getMessage(), Toast.LENGTH_LONG).show());
        }
    }

    private void replaceFile(File src, File dst) throws Exception {
        try {
            Files.move(src.toPath(), dst.toPath(),
                StandardCopyOption.REPLACE_EXISTING,
                StandardCopyOption.ATOMIC_MOVE);
        } catch (java.nio.file.AtomicMoveNotSupportedException e) {
            Files.move(src.toPath(), dst.toPath(), StandardCopyOption.REPLACE_EXISTING);
        }
    }

    private void rememberAndSyncModsFolder(Uri uri, int flags) {
        int takeFlags = flags & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        try {
            getContentResolver().takePersistableUriPermission(uri, takeFlags);
        } catch (Exception e) {
            Log.w(TAG, "Could not persist mods folder permission for " + uri, e);
        }

        Toast.makeText(this, "Importing mods...", Toast.LENGTH_SHORT).show();
        new Thread(() -> importModsFolder(uri), "ISSD-Mods-Import").start();
    }

    private void importModsFolder(Uri uri) {
        File ext = getExternalFilesDir(null);
        File targetDir = ext != null ? ext : getFilesDir();
        File modsDir = new File(targetDir, "mods");
        if (syncModsFolder(uri, modsDir)) {
            getSharedPreferences(PREFS, MODE_PRIVATE)
                .edit()
                .putString(PREF_MODS_TREE_URI, uri.toString())
                .apply();
            runOnUiThread(() -> {
                Toast.makeText(this, "Mods folder imported. Restarting...", Toast.LENGTH_SHORT).show();
                recreate();
            });
        } else {
            getSharedPreferences(PREFS, MODE_PRIVATE)
                .edit()
                .remove(PREF_MODS_TREE_URI)
                .apply();
            runOnUiThread(() -> Toast.makeText(this, "Could not import mods from that folder", Toast.LENGTH_LONG).show());
        }
    }

    private void syncConfiguredModsFolder(File modsDir) {
        SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        String uriText = prefs.getString(PREF_MODS_TREE_URI, null);
        if (uriText == null || uriText.isEmpty()) {
            return;
        }
        syncModsFolder(Uri.parse(uriText), modsDir);
    }

    private boolean syncModsFolder(Uri treeUri, File modsDir) {
        File parent = modsDir.getParentFile();
        if (parent == null) return false;
        File staging = new File(parent, "mods.importing");
        try {
            deleteRecursively(staging);
            if (!staging.mkdirs()) return false;
            copyDocumentTree(treeUri, DocumentsContract.getTreeDocumentId(treeUri), staging);
            deleteRecursively(modsDir);
            if (!staging.renameTo(modsDir)) {
                copyDirectory(staging, modsDir);
                deleteRecursively(staging);
            }
            Log.i(TAG, "Mods imported from " + treeUri + " to " + modsDir.getAbsolutePath());
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Failed to sync mods folder " + treeUri, e);
            deleteRecursively(staging);
            return false;
        }
    }

    private void copyDocumentTree(Uri treeUri, String documentId, File dstDir) throws Exception {
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, documentId);
        String[] projection = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        try (Cursor cursor = getContentResolver().query(childrenUri, projection, null, null, null)) {
            if (cursor == null) {
                throw new IllegalStateException("Could not read selected mods folder");
            }
            int idCol = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DOCUMENT_ID);
            int nameCol = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_DISPLAY_NAME);
            int typeCol = cursor.getColumnIndexOrThrow(DocumentsContract.Document.COLUMN_MIME_TYPE);
            while (cursor.moveToNext()) {
                String childId = cursor.getString(idCol);
                String name = sanitizeFilename(cursor.getString(nameCol));
                String mime = cursor.getString(typeCol);
                if (name.isEmpty()) continue;
                File dst = new File(dstDir, name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    if (!dst.exists() && !dst.mkdirs()) {
                        throw new IllegalStateException("Could not create " + dst);
                    }
                    copyDocumentTree(treeUri, childId, dst);
                } else {
                    copyDocumentFile(DocumentsContract.buildDocumentUriUsingTree(treeUri, childId), dst);
                }
            }
        }
    }

    private void copyDocumentFile(Uri uri, File dst) throws Exception {
        File parent = dst.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            throw new IllegalStateException("Could not create " + parent);
        }
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(dst)) {
            if (in == null) {
                throw new IllegalStateException("Could not open " + uri);
            }
            byte[] buf = new byte[65536];
            int len;
            while ((len = in.read(buf)) > 0) {
                out.write(buf, 0, len);
            }
        }
    }

    private void copyDirectory(File src, File dst) throws Exception {
        if (!dst.exists() && !dst.mkdirs()) {
            throw new IllegalStateException("Could not create " + dst);
        }
        File[] children = src.listFiles();
        if (children == null) return;
        for (File child : children) {
            File out = new File(dst, child.getName());
            if (child.isDirectory()) {
                copyDirectory(child, out);
            } else {
                try (InputStream in = new java.io.FileInputStream(child);
                     OutputStream os = new FileOutputStream(out)) {
                    byte[] buf = new byte[65536];
                    int len;
                    while ((len = in.read(buf)) > 0) {
                        os.write(buf, 0, len);
                    }
                }
            }
        }
    }

    private void deleteRecursively(File file) {
        if (file == null || !file.exists()) return;
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        if (!file.delete()) {
            Log.w(TAG, "Could not delete " + file.getAbsolutePath());
        }
    }

    private String sanitizeFilename(String name) {
        if (name == null) return "";
        return name.replace('/', '_').replace('\\', '_');
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) goImmersive();
    }

    /** Hide the status and navigation bars; they steal touches near the edges. */
    private void goImmersive() {
        View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
          | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
          | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
          | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
          | View.SYSTEM_UI_FLAG_FULLSCREEN
          | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    /**
     * Hand the native side its storage locations.
     *
     * @param externalDir app-private external storage: ROM and config live
     *                    here, reachable over USB without permissions
     * @param internalDir app-private internal storage, for saves
     */
    private static native void nativeSetPaths(String externalDir, String internalDir);
}
