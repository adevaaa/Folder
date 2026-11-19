package re.naruto.game;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.view.KeyEvent;
import android.view.WindowManager;

import org.cocos2dx.lib.Cocos2dxActivity;
import org.cocos2dx.lib.Cocos2dxGLSurfaceView;

public class NarutoSenki extends Cocos2dxActivity{
	protected void onCreate(Bundle savedInstanceState){
		super.onCreate(savedInstanceState);
		getWindow().setFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		requestStorageAccess();
	}

	// Izin penyimpanan untuk folder data /sdcard/Aris. Tanpa izin, game tetap jalan memakai isi APK.
	// Setelah izin diberikan, buka ulang game agar folder Aris dipakai.
	private void requestStorageAccess() {
		try {
			if (Build.VERSION.SDK_INT >= 30) {
				if (!Environment.isExternalStorageManager()) {
					try {
						Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
								Uri.parse("package:" + getPackageName()));
						startActivity(intent);
					} catch (Exception e) {
						startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
					}
				}
			} else if (Build.VERSION.SDK_INT >= 23) {
				if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
					requestPermissions(new String[] {
							Manifest.permission.READ_EXTERNAL_STORAGE,
							Manifest.permission.WRITE_EXTERNAL_STORAGE }, 1001);
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public Cocos2dxGLSurfaceView onCreateGLSurfaceView() {
		return new LuaGLSurfaceView(this);
	}

     static {
         System.loadLibrary("cocos2dcpp");
     }
}

class LuaGLSurfaceView extends Cocos2dxGLSurfaceView{

	public LuaGLSurfaceView(Context context){
		super(context);
	}

	public boolean onKeyDown(int keyCode, KeyEvent event) {
		// exit program when key back is entered
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			android.os.Process.killProcess(android.os.Process.myPid());
		}
		return super.onKeyDown(keyCode, event);
	}
}
