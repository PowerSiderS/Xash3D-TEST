package in.celest.xash3d;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import in.celest.xash3d.cs16.R;
import in.celest.xash3d.cs16.BuildConfig;
import su.xash.fwgslib.CertCheck;
import su.xash.fwgslib.FWGSLib;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;

public class LauncherActivity extends Activity 
{
	public static final String TAG = "LauncherActivity";
	public static final int PAK_VERSION = 7;
	private static final int REQUEST_STORAGE_PERMISSION = 1001;
	private static final int REQUEST_FOLDER_PICK = 1002;

	public static SharedPreferences mPref;

	private EditText editTextPath;
	private TextView textViewPathStatus;
	private LinearLayout cardPermissionWarning;
	private Button buttonGrantPermission;
	private Button buttonSelectFolder;
	private Button buttonDefaultPath;
	private CheckBox checkBoxZBot;
	private CheckBox checkBoxYaPB;
	private CheckBox checkBoxImmersive;
	private CheckBox checkBoxResize;
	private EditText editTextCmdArgs;
	private TextView toggleAdvanced;
	private LinearLayout layoutAdvanced;
	private CheckBox checkBoxVolume;
	private Spinner spinnerPixelFormat;

	// Resolution settings
	private CheckBox checkBoxResolution;
	private RadioGroup scaleGroup;
	private RadioButton radioScale;
	private RadioButton radioCustom;
	private EditText resWidth;
	private EditText resHeight;
	private EditText resScale;
	private TextView resResult;
	private int mEngineWidth;
	private int mEngineHeight;

	@Override
	protected void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);

		if (CertCheck.dumbAntiPDALifeCheck(this)) 
		{
			finish();
			return;
		}

		setContentView(R.layout.activity_launcher);

		mPref = getSharedPreferences("engine", 0);

		// UI elements
		editTextPath = findViewById(R.id.editText_path);
		textViewPathStatus = findViewById(R.id.textView_path_status);
		cardPermissionWarning = findViewById(R.id.card_permission_warning);
		buttonGrantPermission = findViewById(R.id.button_grant_permission);
		buttonSelectFolder = findViewById(R.id.button_select_folder);
		buttonDefaultPath = findViewById(R.id.button_default_path);
		checkBoxZBot = findViewById(R.id.use_zbot);
		checkBoxYaPB = findViewById(R.id.use_yapb);
		checkBoxImmersive = findViewById(R.id.immersive_mode);
		checkBoxResize = findViewById(R.id.enableResizeWorkaround);
		editTextCmdArgs = findViewById(R.id.cmdArgs);
		toggleAdvanced = findViewById(R.id.toggle_advanced);
		layoutAdvanced = findViewById(R.id.layout_advanced_options);
		checkBoxVolume = findViewById(R.id.useVolume);
		spinnerPixelFormat = findViewById(R.id.pixelSpinner);

		// Resolution elements
		checkBoxResolution = findViewById(R.id.resolution);
		scaleGroup = findViewById(R.id.scale_group);
		radioScale = findViewById(R.id.resolution_scale_r);
		radioCustom = findViewById(R.id.resolution_custom_r);
		resWidth = findViewById(R.id.resolution_width);
		resHeight = findViewById(R.id.resolution_height);
		resScale = findViewById(R.id.resolution_scale);
		resResult = findViewById(R.id.resolution_result);

		// Initialize values from preferences
		String defaultPath = FWGSLib.getDefaultXashPath(this);
		String savedPath = mPref.getString("basedir", defaultPath);
		editTextPath.setText(savedPath);

		checkBoxZBot.setChecked(mPref.getBoolean("zbots", true));
		checkBoxYaPB.setChecked(mPref.getBoolean("yapbs", false));
		checkBoxImmersive.setChecked(mPref.getBoolean("immersive_mode", true));
		checkBoxResize.setChecked(mPref.getBoolean("enableResizeWorkaround", true));
		editTextCmdArgs.setText(mPref.getString("argv", "-console"));

		checkBoxVolume.setChecked(mPref.getBoolean("usevolume", true));

		final String[] pixelFormats = {
			"32 bit (RGBA8888)",
			"24 bit (RGB888)",
			"16 bit (RGB565)",
			"16 bit (RGBA5551)",
			"16 bit (RGBA4444)",
			"8 bit (RGB332)"
		};
		ArrayAdapter<String> pixelAdapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, pixelFormats);
		spinnerPixelFormat.setAdapter(pixelAdapter);
		spinnerPixelFormat.setSelection(mPref.getInt("pixelformat", 0));

		// Compute screen dimensions
		DisplayMetrics metrics = new DisplayMetrics();
		getWindowManager().getDefaultDisplay().getMetrics(metrics);
		if (FWGSLib.isLandscapeOrientation(this)) 
		{
			mEngineWidth = metrics.widthPixels;
			mEngineHeight = metrics.heightPixels;
		} 
		else 
		{
			mEngineWidth = metrics.heightPixels;
			mEngineHeight = metrics.widthPixels;
		}

		resWidth.setText(String.valueOf(mPref.getInt("resolution_width", mEngineWidth > 0 ? mEngineWidth : 854)));
		resHeight.setText(String.valueOf(mPref.getInt("resolution_height", mEngineHeight > 0 ? mEngineHeight : 480)));
		resScale.setText(String.valueOf(mPref.getFloat("resolution_scale", 2.0f)));

		TextWatcher resTextChangeWatcher = new TextWatcher() 
		{
			@Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
			@Override public void onTextChanged(CharSequence s, int start, int before, int count) { updateResolutionResult(); }
			@Override public void afterTextChanged(Editable s) {}
		};
		resWidth.addTextChangedListener(resTextChangeWatcher);
		resHeight.addTextChangedListener(resTextChangeWatcher);
		resScale.addTextChangedListener(resTextChangeWatcher);

		if (mPref.getBoolean("resolution_custom", false)) 
		{
			radioCustom.setChecked(true);
		} 
		else 
		{
			radioScale.setChecked(true);
		}

		radioCustom.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() 
		{
			@Override
			public void onCheckedChanged(CompoundButton v, boolean isChecked) 
			{
				updateResolutionResult();
				toggleResolutionFields();
			}
		});

		checkBoxResolution.setChecked(mPref.getBoolean("resolution_fixed", false));
		checkBoxResolution.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() 
		{
			@Override
			public void onCheckedChanged(CompoundButton v, boolean isChecked) 
			{
				scaleGroup.setVisibility(isChecked ? View.VISIBLE : View.GONE);
			}
		});

		scaleGroup.setVisibility(checkBoxResolution.isChecked() ? View.VISIBLE : View.GONE);
		toggleResolutionFields();
		updateResolutionResult();

		// Advanced options toggle
		toggleAdvanced.setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				if (layoutAdvanced.getVisibility() == View.VISIBLE) 
				{
					layoutAdvanced.setVisibility(View.GONE);
					toggleAdvanced.setText("Advanced Engine Options ▼");
				} 
				else 
				{
					layoutAdvanced.setVisibility(View.VISIBLE);
					toggleAdvanced.setText("Advanced Engine Options ▲");
				}
			}
		});

		// Folder selection button - launches full-screen ZArchiver-style browser
		buttonSelectFolder.setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				if (!hasStoragePermission()) 
				{
					requestStoragePermission();
				} 
				else 
				{
					showDirectoryChooser();
				}
			}
		});

		// Reset default path button
		buttonDefaultPath.setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				String def = FWGSLib.getDefaultXashPath(LauncherActivity.this);
				editTextPath.setText(def);
				updatePathStatus(def);
			}
		});

		// Permission grant button
		buttonGrantPermission.setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				requestStoragePermission();
			}
		});

		// Live path text change listener
		editTextPath.addTextChangedListener(new TextWatcher() 
		{
			@Override
			public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

			@Override
			public void onTextChanged(CharSequence s, int start, int before, int count) {}

			@Override
			public void afterTextChanged(Editable s) 
			{
				updatePathStatus(s.toString().trim());
			}
		});

		// Check permission and update UI
		checkAndShowPermissionWarning();
		updatePathStatus(savedPath);

		// Extract assets/extras in background
		extractAssetsIfNeeded();
	}

	@Override
	protected void onResume() 
	{
		super.onResume();
		checkAndShowPermissionWarning();
		if (editTextPath != null) 
		{
			updatePathStatus(editTextPath.getText().toString().trim());
		}
	}

	private void updateResolutionResult() 
	{
		if (resResult == null || radioCustom == null) return;
		int w, h;
		if (radioCustom.isChecked()) 
		{
			w = getCustomEngineWidth();
			h = getCustomEngineHeight();
		} 
		else 
		{
			float scale = getResolutionScale();
			if (scale <= 0.1f) scale = 1.0f;
			w = (int) ((float) mEngineWidth / scale);
			h = (int) ((float) mEngineHeight / scale);
		}
		resResult.setText(getString(R.string.resolution_result) + w + "x" + h);
	}

	private void toggleResolutionFields() 
	{
		if (radioCustom == null) return;
		boolean isCustom = radioCustom.isChecked();
		resWidth.setEnabled(isCustom);
		resHeight.setEnabled(isCustom);
		resScale.setEnabled(!isCustom);
	}

	private float getResolutionScale() 
	{
		try 
		{
			return Float.parseFloat(resScale.getText().toString());
		} 
		catch (Exception e) 
		{
			return 2.0f;
		}
	}

	private int getCustomEngineWidth() 
	{
		try 
		{
			return Integer.parseInt(resWidth.getText().toString());
		} 
		catch (Exception e) 
		{
			return mEngineWidth > 0 ? mEngineWidth : 854;
		}
	}

	private int getCustomEngineHeight() 
	{
		try 
		{
			return Integer.parseInt(resHeight.getText().toString());
		} 
		catch (Exception e) 
		{
			return mEngineHeight > 0 ? mEngineHeight : 480;
		}
	}

	private boolean hasStoragePermission() 
	{
		if (Build.VERSION.SDK_INT >= 30) // Android 11+
		{
			try 
			{
				Method m = Environment.class.getMethod("isExternalStorageManager");
				Boolean res = (Boolean) m.invoke(null);
				if (res != null && res) 
				{
					return true;
				}
			} 
			catch (Exception ignored) {}
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) 
		{
			return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
		}
		return true;
	}

	private void requestStoragePermission() 
	{
		if (Build.VERSION.SDK_INT >= 30) 
		{
			try 
			{
				Intent intent = new Intent("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION");
				intent.setData(Uri.parse("package:" + getPackageName()));
				startActivityForResult(intent, REQUEST_STORAGE_PERMISSION);
				return;
			} 
			catch (Exception e) 
			{
				try 
				{
					Intent intent = new Intent("android.settings.MANAGE_ALL_FILES_ACCESS_PERMISSION");
					startActivityForResult(intent, REQUEST_STORAGE_PERMISSION);
					return;
				} 
				catch (Exception ignored) {}
			}
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) 
		{
			requestPermissions(new String[]{
				Manifest.permission.READ_EXTERNAL_STORAGE,
				Manifest.permission.WRITE_EXTERNAL_STORAGE
			}, REQUEST_STORAGE_PERMISSION);
		}
	}

	private void checkAndShowPermissionWarning() 
	{
		if (cardPermissionWarning == null) return;

		if (hasStoragePermission()) 
		{
			cardPermissionWarning.setVisibility(View.GONE);
		} 
		else 
		{
			cardPermissionWarning.setVisibility(View.VISIBLE);
		}
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) 
	{
		super.onRequestPermissionsResult(requestCode, permissions, grantResults);
		if (requestCode == REQUEST_STORAGE_PERMISSION) 
		{
			checkAndShowPermissionWarning();
			if (editTextPath != null) 
			{
				updatePathStatus(editTextPath.getText().toString().trim());
			}
		}
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) 
	{
		super.onActivityResult(requestCode, resultCode, data);
		if (requestCode == REQUEST_STORAGE_PERMISSION) 
		{
			checkAndShowPermissionWarning();
			if (editTextPath != null) 
			{
				updatePathStatus(editTextPath.getText().toString().trim());
			}
		} 
		else if (requestCode == REQUEST_FOLDER_PICK && resultCode == RESULT_OK && data != null) 
		{
			String path = data.getStringExtra("path");
			if (path != null && !path.trim().isEmpty()) 
			{
				editTextPath.setText(path.trim());
				updatePathStatus(path.trim());
				mPref.edit().putString("basedir", path.trim()).apply();
				Toast.makeText(this, "Selected: " + path.trim(), Toast.LENGTH_SHORT).show();
			}
		}
	}

	private void updatePathStatus(String path) 
	{
		if (textViewPathStatus == null) return;

		if (path == null || path.isEmpty()) 
		{
			textViewPathStatus.setText("⚠ Please set a game directory");
			textViewPathStatus.setTextColor(0xFFFFAA00);
			return;
		}

		File dir = new File(path);
		if (!dir.exists()) 
		{
			textViewPathStatus.setText("⚠ Directory does not exist");
			textViewPathStatus.setTextColor(0xFFFF5555);
			return;
		}

		File cstrike = new File(dir, "cstrike");
		File valve = new File(dir, "valve");

		if (cstrike.isDirectory() && valve.isDirectory()) 
		{
			textViewPathStatus.setText("✓ cstrike and valve folders found!");
			textViewPathStatus.setTextColor(0xFF55FF55);
		} 
		else if (cstrike.isDirectory()) 
		{
			textViewPathStatus.setText("✓ cstrike folder found");
			textViewPathStatus.setTextColor(0xFF77FF77);
		} 
		else if (valve.isDirectory()) 
		{
			textViewPathStatus.setText("✓ valve folder found");
			textViewPathStatus.setTextColor(0xFF77FF77);
		} 
		else 
		{
			textViewPathStatus.setText("⚠ Neither cstrike nor valve found here");
			textViewPathStatus.setTextColor(0xFFFFAA00);
		}
	}

	private void showDirectoryChooser() 
	{
		Intent intent = new Intent(this, FolderPickerActivity.class);
		intent.putExtra("initial_path", editTextPath.getText().toString().trim());
		startActivityForResult(intent, REQUEST_FOLDER_PICK);
	}

	/**
	 * Extract extras.pak from assets to internal storage so the game engine loads
	 * touch buttons, presets, and configs.
	 */
	private void extractAssetsIfNeeded() 
	{
		new Thread(new Runnable() 
		{
			@Override
			public void run() 
			{
				try 
				{
					File pakFile = new File(getFilesDir(), "extras.pak");
					int currentVersion = mPref.getInt("pakversion", 0);

					if (!pakFile.exists() || currentVersion != PAK_VERSION) 
					{
						Log.i(TAG, "Extracting extras.pak from assets...");
						InputStream is = getAssets().open("extras.pak");
						OutputStream os = new FileOutputStream(pakFile);
						byte[] buf = new byte[8192];
						int len;
						while ((len = is.read(buf)) > 0) 
						{
							os.write(buf, 0, len);
						}
						os.flush();
						os.close();
						is.close();

						mPref.edit().putInt("pakversion", PAK_VERSION).apply();
						Log.i(TAG, "extras.pak extraction completed.");
					}
				} 
				catch (Exception e) 
				{
					// extras.pak not bundled or skipped
					Log.d(TAG, "extras.pak extraction skipped");
				}
			}
		}).start();
	}

	public void startXash(View view) 
	{
		String chosenPath = editTextPath.getText().toString().trim();
		if (chosenPath.isEmpty()) 
		{
			chosenPath = FWGSLib.getDefaultXashPath(this);
		}

		// Ensure storage permission
		if (!hasStoragePermission()) 
		{
			Toast.makeText(this, "Storage permission is required to launch game!", Toast.LENGTH_LONG).show();
			requestStoragePermission();
			return;
		}

		String argv = editTextCmdArgs.getText().toString().trim();

		// Always ensure -log is present directly without needing an option
		if (!argv.contains("-log")) 
		{
			argv = argv.isEmpty() ? "-log" : argv + " -log";
		}

		// Bot configuration
		if (checkBoxZBot.isChecked()) 
		{
			if (!argv.contains("-bots")) 
			{
				argv = argv + " -bots";
			}
		}

		if (checkBoxYaPB.isChecked()) 
		{
			String libDir = getFilesDir().getAbsolutePath().replace("/files", "/lib");
			File yapb = new File(libDir, "libyapb.so");
			if (yapb.exists()) 
			{
				argv = argv + " -dll " + yapb.getAbsolutePath();
			}
		}

		// Save preferences
		SharedPreferences.Editor editor = mPref.edit();
		editor.putString("basedir", chosenPath);
		editor.putString("argv", argv);
		editor.putBoolean("zbots", checkBoxZBot.isChecked());
		editor.putBoolean("yapbs", checkBoxYaPB.isChecked());
		editor.putBoolean("immersive_mode", checkBoxImmersive.isChecked());
		editor.putBoolean("enableResizeWorkaround", checkBoxResize.isChecked());
		editor.putBoolean("resolution_fixed", checkBoxResolution.isChecked());
		editor.putBoolean("resolution_custom", radioCustom.isChecked());
		editor.putFloat("resolution_scale", getResolutionScale());
		editor.putInt("resolution_width", getCustomEngineWidth());
		editor.putInt("resolution_height", getCustomEngineHeight());
		editor.putBoolean("usevolume", checkBoxVolume.isChecked());
		editor.putInt("pixelformat", spinnerPixelFormat.getSelectedItemPosition());
		editor.apply();

		// Launch XashActivity
		Intent intent = new Intent(this, XashActivity.class);
		intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		intent.putExtra("basedir", chosenPath);
		intent.putExtra("gamedir", "cstrike");
		intent.putExtra("argv", argv);
		File pakFile = new File(getFilesDir(), "extras.pak");
		if (pakFile.exists()) 
		{
			intent.putExtra("pakfile", pakFile.getAbsolutePath());
		}

		startActivity(intent);
	}
}
