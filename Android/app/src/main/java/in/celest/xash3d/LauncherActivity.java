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
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
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
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class LauncherActivity extends Activity 
{
	public static final String TAG = "LauncherActivity";
	public static final int PAK_VERSION = 5;
	private static final int REQUEST_STORAGE_PERMISSION = 1001;

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
	private EditText editTextCmdArgs;
	private TextView toggleAdvanced;
	private LinearLayout layoutAdvanced;
	private CheckBox checkBoxVolume;
	private CheckBox checkBoxResize;
	private Spinner spinnerPixelFormat;

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
		editTextCmdArgs = findViewById(R.id.cmdArgs);
		toggleAdvanced = findViewById(R.id.toggle_advanced);
		layoutAdvanced = findViewById(R.id.layout_advanced_options);
		checkBoxVolume = findViewById(R.id.useVolume);
		checkBoxResize = findViewById(R.id.enableResizeWorkaround);
		spinnerPixelFormat = findViewById(R.id.pixelSpinner);

		// Initialize values from preferences
		String defaultPath = FWGSLib.getDefaultXashPath(this);
		String savedPath = mPref.getString("basedir", defaultPath);
		editTextPath.setText(savedPath);

		checkBoxZBot.setChecked(mPref.getBoolean("zbots", true));
		checkBoxYaPB.setChecked(mPref.getBoolean("yapbs", false));
		checkBoxImmersive.setChecked(mPref.getBoolean("immersive_mode", true));
		editTextCmdArgs.setText(mPref.getString("argv", "-console"));

		checkBoxVolume.setChecked(mPref.getBoolean("usevolume", true));
		checkBoxResize.setChecked(mPref.getBoolean("enableResizeWorkaround", true));

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

		// Folder selection button
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

	private boolean hasStoragePermission() 
	{
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) 
		{
			return Environment.isExternalStorageManager();
		} 
		else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) 
		{
			return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
		}
		return true;
	}

	private void requestStoragePermission() 
	{
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) 
		{
			try 
			{
				Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
				intent.setData(Uri.parse("package:" + getPackageName()));
				startActivityForResult(intent, REQUEST_STORAGE_PERMISSION);
			} 
			catch (Exception e) 
			{
				Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
				try 
				{
					startActivityForResult(intent, REQUEST_STORAGE_PERMISSION);
				} 
				catch (Exception ex) 
				{
					Toast.makeText(this, "Please grant storage permission in Settings", Toast.LENGTH_LONG).show();
				}
			}
		} 
		else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) 
		{
			requestPermissions(new String[]{
				Manifest.permission.READ_EXTERNAL_STORAGE,
				Manifest.permission.WRITE_EXTERNAL_STORAGE
			}, REQUEST_STORAGE_PERMISSION);
		}
	}

	private void checkAndShowPermissionWarning() 
	{
		if (!hasStoragePermission()) 
		{
			cardPermissionWarning.setVisibility(View.VISIBLE);
		} 
		else 
		{
			cardPermissionWarning.setVisibility(View.GONE);
		}
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) 
	{
		super.onRequestPermissionsResult(requestCode, permissions, grantResults);
		if (requestCode == REQUEST_STORAGE_PERMISSION) 
		{
			if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) 
			{
				cardPermissionWarning.setVisibility(View.GONE);
				Toast.makeText(this, "Storage permission granted!", Toast.LENGTH_SHORT).show();
				if (editTextPath != null) 
				{
					updatePathStatus(editTextPath.getText().toString().trim());
				}
			} 
			else 
			{
				Toast.makeText(this, "Permission denied. Game resources might not load.", Toast.LENGTH_LONG).show();
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
	}

	private void updatePathStatus(String path) 
	{
		if (path == null || path.isEmpty()) 
		{
			textViewPathStatus.setText("⚠ No path specified");
			textViewPathStatus.setTextColor(0xffff8800);
			return;
		}

		File dir = new File(path);
		if (!dir.exists()) 
		{
			textViewPathStatus.setText("⚠ Directory does not exist (will be created)");
			textViewPathStatus.setTextColor(0xffffaa00);
			return;
		}

		File cstrike = new File(dir, "cstrike");
		File valve = new File(dir, "valve");

		if (cstrike.exists() && valve.exists()) 
		{
			textViewPathStatus.setText("✓ cstrike & valve folders found!");
			textViewPathStatus.setTextColor(0xff55ff55);
		} 
		else if (cstrike.exists()) 
		{
			textViewPathStatus.setText("✓ cstrike folder found!");
			textViewPathStatus.setTextColor(0xff55ff55);
		} 
		else if (valve.exists()) 
		{
			textViewPathStatus.setText("⚠ valve found, but cstrike folder is missing!");
			textViewPathStatus.setTextColor(0xffffaa00);
		} 
		else 
		{
			textViewPathStatus.setText("⚠ Neither cstrike nor valve found in this folder");
			textViewPathStatus.setTextColor(0xffff8800);
		}
	}

	/**
	 * Shows an interactive folder chooser dialog to browse and select the CS 1.6 game directory.
	 */
	private void showDirectoryChooser() 
	{
		String currentPath = editTextPath.getText().toString().trim();
		File currentDir = new File(currentPath);
		if (!currentDir.exists() || !currentDir.isDirectory()) 
		{
			currentDir = Environment.getExternalStorageDirectory();
			if (currentDir == null || !currentDir.exists()) 
			{
				currentDir = new File("/sdcard");
			}
		}

		browseFolderDialog(currentDir);
	}

	private void browseFolderDialog(final File dir) 
	{
		final List<File> subFolders = new ArrayList<>();
		final List<String> displayNames = new ArrayList<>();

		// Option to go up one directory
		final boolean hasParent = (dir.getParentFile() != null && dir.getParentFile().canRead());
		if (hasParent) 
		{
			displayNames.add("📁 .. [Parent Directory]");
			subFolders.add(dir.getParentFile());
		}

		// List subfolders
		File[] files = dir.listFiles();
		if (files != null) 
		{
			List<File> dirList = new ArrayList<>();
			for (File f : files) 
			{
				if (f.isDirectory() && !f.getName().startsWith(".")) 
				{
					dirList.add(f);
				}
			}
			Collections.sort(dirList, new java.util.Comparator<File>() 
			{
				@Override
				public int compare(File a, File b) 
				{
					return a.getName().compareToIgnoreCase(b.getName());
				}
			});

			for (File d : dirList) 
			{
				boolean hasCS = new File(d, "cstrike").exists();
				String prefix = hasCS ? "⭐ " : "📁 ";
				displayNames.add(prefix + d.getName() + (hasCS ? " (CS 1.6)" : ""));
				subFolders.add(d);
			}
		}

		// Check if current directory has cstrike
		boolean cstrikeHere = new File(dir, "cstrike").exists();
		String title = dir.getAbsolutePath() + (cstrikeHere ? "\n✓ cstrike found here!" : "");

		AlertDialog.Builder builder = new AlertDialog.Builder(this);
		builder.setTitle("Select Game Data Folder");
		builder.setMessage(title);

		ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, displayNames);

		builder.setAdapter(adapter, new DialogInterface.OnClickListener() 
		{
			@Override
			public void onClick(DialogInterface dialog, int which) 
			{
				File selected = subFolders.get(which);
				browseFolderDialog(selected);
			}
		});

		builder.setPositiveButton("Set Current Folder", new DialogInterface.OnClickListener() 
		{
			@Override
			public void onClick(DialogInterface dialog, int which) 
			{
				String selectedPath = dir.getAbsolutePath();
				editTextPath.setText(selectedPath);
				updatePathStatus(selectedPath);
				mPref.edit().putString("basedir", selectedPath).apply();
				Toast.makeText(LauncherActivity.this, "Selected: " + selectedPath, Toast.LENGTH_SHORT).show();
			}
		});

		builder.setNegativeButton("Cancel", null);

		builder.setNeutralButton("New Folder", new DialogInterface.OnClickListener() 
		{
			@Override
			public void onClick(DialogInterface dialog, int which) 
			{
				showCreateFolderDialog(dir);
			}
		});

		builder.create().show();
	}

	private void showCreateFolderDialog(final File parentDir) 
	{
		final EditText input = new EditText(this);
		input.setHint("xash");
		new AlertDialog.Builder(this)
			.setTitle("Create Folder in " + parentDir.getName())
			.setView(input)
			.setPositiveButton("Create", new DialogInterface.OnClickListener() 
			{
				@Override
				public void onClick(DialogInterface d, int which) 
				{
					String name = input.getText().toString().trim();
					if (!name.isEmpty()) 
					{
						File newDir = new File(parentDir, name);
						if (newDir.mkdirs() || newDir.exists()) 
						{
							browseFolderDialog(newDir);
						} 
						else 
						{
							Toast.makeText(LauncherActivity.this, "Failed to create directory", Toast.LENGTH_SHORT).show();
						}
					}
				}
			})
			.setNegativeButton("Cancel", null)
			.show();
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
				Log.w(TAG, "extras.pak not found or extraction skipped: " + e.getMessage());
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
		editor.putBoolean("usevolume", checkBoxVolume.isChecked());
		editor.putBoolean("enableResizeWorkaround", checkBoxResize.isChecked());
		editor.putInt("pixelformat", spinnerPixelFormat.getSelectedItemPosition());
		editor.apply();

		// Launch XashActivity
		Intent intent = new Intent(this, XashActivity.class);
		intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		intent.putExtra("basedir", chosenPath);
		intent.putExtra("gamedir", "cstrike");
		intent.putExtra("argv", argv);
		intent.putExtra("pakfile", getFilesDir().getAbsolutePath() + "/extras.pak");

		startActivity(intent);
	}
}
