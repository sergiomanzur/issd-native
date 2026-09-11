package com.issdnative;

import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * Android entry point.
 *
 * SDLActivity does the heavy lifting - window, GL surface, input, audio and
 * the native thread that calls SDL_main. This subclass only adds what a
 * handheld build needs: the libraries to load, immersive fullscreen, and a
 * writable place for the ROM and config that does not need storage
 * permissions.
 */
public class ISSDActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        // Order matters: libmain.so links against libSDL2.so.
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // The native side reads these instead of guessing at Android storage
        // layout. getExternalFilesDir is app-private but visible over USB and
        // to file managers, so a user can drop their own ROM in without any
        // runtime permission prompt.
        java.io.File ext = getExternalFilesDir(null);
        nativeSetPaths(
            (ext != null ? ext : getFilesDir()).getAbsolutePath(),
            getFilesDir().getAbsolutePath());

        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
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
