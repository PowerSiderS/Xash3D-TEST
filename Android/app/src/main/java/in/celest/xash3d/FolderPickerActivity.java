package in.celest.xash3d;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import in.celest.xash3d.cs16.R;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class FolderPickerActivity extends Activity 
{
	private File mCurrentDir;
	private TextView mTvCurrentPath;
	private TextView mTvStatus;
	private ListView mListView;
	private TextView mTvEmpty;
	private Button mBtnSelectThisFolder;
	private FolderAdapter mAdapter;
	private List<FileItem> mItems = new ArrayList<>();

	public static class FileItem 
	{
		public final File file;
		public final String name;
		public final boolean isParent;
		public final boolean hasCstrike;
		public final int childCount;

		public FileItem(File file, String name, boolean isParent, boolean hasCstrike, int childCount) 
		{
			this.file = file;
			this.name = name;
			this.isParent = isParent;
			this.hasCstrike = hasCstrike;
			this.childCount = childCount;
		}
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_folder_picker);

		mTvCurrentPath = findViewById(R.id.tv_current_path);
		mTvStatus = findViewById(R.id.tv_cs16_status);
		mListView = findViewById(R.id.list_folders);
		mTvEmpty = findViewById(R.id.tv_empty);
		mBtnSelectThisFolder = findViewById(R.id.btn_select_this_folder);

		mAdapter = new FolderAdapter();
		mListView.setAdapter(mAdapter);

		// Determine initial directory
		String initialPath = getIntent().getStringExtra("initial_path");
		File startDir = null;
		if (initialPath != null && !initialPath.trim().isEmpty()) 
		{
			File candidate = new File(initialPath.trim());
			if (candidate.exists() && candidate.isDirectory()) 
			{
				startDir = candidate;
			}
		}

		if (startDir == null) 
		{
			File ext = Environment.getExternalStorageDirectory();
			File xash = new File(ext, "xash");
			if (xash.exists() && xash.isDirectory()) 
			{
				startDir = xash;
			} 
			else if (ext.exists()) 
			{
				startDir = ext;
			} 
			else 
			{
				startDir = new File("/");
			}
		}

		// Button listeners
		findViewById(R.id.btn_back).setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				finish();
			}
		});

		findViewById(R.id.btn_shortcut_internal).setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				navigateTo(Environment.getExternalStorageDirectory());
			}
		});

		findViewById(R.id.btn_shortcut_root).setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				navigateTo(new File("/"));
			}
		});

		findViewById(R.id.btn_up).setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				goUp();
			}
		});

		findViewById(R.id.btn_new_folder).setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				showCreateFolderDialog();
			}
		});

		mBtnSelectThisFolder.setOnClickListener(new View.OnClickListener() 
		{
			@Override
			public void onClick(View v) 
			{
				if (mCurrentDir != null) 
				{
					Intent data = new Intent();
					data.putExtra("path", mCurrentDir.getAbsolutePath());
					setResult(RESULT_OK, data);
					finish();
				}
			}
		});

		mListView.setOnItemClickListener(new AdapterView.OnItemClickListener() 
		{
			@Override
			public void onItemClick(AdapterView<?> parent, View view, int position, long id) 
			{
				FileItem item = mItems.get(position);
				if (item.isParent) 
				{
					goUp();
				} 
				else if (item.file != null && item.file.isDirectory()) 
				{
					navigateTo(item.file);
				}
			}
		});

		navigateTo(startDir);
	}

	private void goUp() 
	{
		if (mCurrentDir != null) 
		{
			File parent = mCurrentDir.getParentFile();
			if (parent != null && parent.exists() && parent.canRead()) 
			{
				navigateTo(parent);
			} 
			else 
			{
				Toast.makeText(this, "Already at top level directory", Toast.LENGTH_SHORT).show();
			}
		}
	}

	private void navigateTo(File dir) 
	{
		if (dir == null || !dir.exists() || !dir.isDirectory()) 
		{
			Toast.makeText(this, "Cannot open folder", Toast.LENGTH_SHORT).show();
			return;
		}

		mCurrentDir = dir;
		mTvCurrentPath.setText(dir.getAbsolutePath());

		loadFolderContents();
		updateStatus();
	}

	private void loadFolderContents() 
	{
		mItems.clear();

		File parent = mCurrentDir.getParentFile();
		if (parent != null && parent.exists()) 
		{
			mItems.add(new FileItem(parent, ".. (Up to " + (parent.getName().isEmpty() ? "/" : parent.getName()) + ")", true, false, 0));
		}

		File[] files = mCurrentDir.listFiles();
		List<FileItem> dirs = new ArrayList<>();

		if (files != null) 
		{
			for (File f : files) 
			{
				if (f.isDirectory() && !f.isHidden()) 
				{
					boolean hasCstrike = new File(f, "cstrike").isDirectory() || new File(f, "valve").isDirectory();
					int count = 0;
					try 
					{
						File[] sub = f.listFiles();
						if (sub != null) count = sub.length;
					} 
					catch (Exception ignored) {}

					dirs.add(new FileItem(f, f.getName(), false, hasCstrike, count));
				}
			}

			Collections.sort(dirs, new Comparator<FileItem>() 
			{
				@Override
				public int compare(FileItem o1, FileItem o2) 
				{
					// If one has cstrike, list it first!
					if (o1.hasCstrike && !o2.hasCstrike) return -1;
					if (!o1.hasCstrike && o2.hasCstrike) return 1;
					return o1.name.compareToIgnoreCase(o2.name);
				}
			});

			mItems.addAll(dirs);
		}

		mAdapter.notifyDataSetChanged();

		if (mItems.isEmpty()) 
		{
			mListView.setVisibility(View.GONE);
			mTvEmpty.setVisibility(View.VISIBLE);
		} 
		else 
		{
			mListView.setVisibility(View.VISIBLE);
			mTvEmpty.setVisibility(View.GONE);
		}
	}

	private void updateStatus() 
	{
		if (mCurrentDir == null) return;

		boolean cstrike = new File(mCurrentDir, "cstrike").isDirectory();
		boolean valve = new File(mCurrentDir, "valve").isDirectory();

		if (cstrike && valve) 
		{
			mTvStatus.setText("✓ 'cstrike' and 'valve' found in this directory!");
			mTvStatus.setTextColor(0xFF55FF55);
		} 
		else if (cstrike) 
		{
			mTvStatus.setText("✓ 'cstrike' folder found here");
			mTvStatus.setTextColor(0xFF77FF77);
		} 
		else if (valve) 
		{
			mTvStatus.setText("✓ 'valve' folder found here");
			mTvStatus.setTextColor(0xFF77FF77);
		} 
		else 
		{
			mTvStatus.setText("⚠ Neither 'cstrike' nor 'valve' found in this directory");
			mTvStatus.setTextColor(0xFFFFAA00);
		}

		mBtnSelectThisFolder.setText("✔ SELECT THIS FOLDER (" + mCurrentDir.getName() + ")");
	}

	private void showCreateFolderDialog() 
	{
		final EditText input = new EditText(this);
		input.setHint("xash");
		new AlertDialog.Builder(this)
			.setTitle("Create New Folder")
			.setMessage("Creating folder inside: " + mCurrentDir.getAbsolutePath())
			.setView(input)
			.setPositiveButton("Create", new DialogInterface.OnClickListener() 
			{
				@Override
				public void onClick(DialogInterface dialog, int which) 
				{
					String name = input.getText().toString().trim();
					if (!name.isEmpty()) 
					{
						File newDir = new File(mCurrentDir, name);
						if (newDir.mkdirs() || newDir.exists()) 
						{
							navigateTo(newDir);
						} 
						else 
						{
							Toast.makeText(FolderPickerActivity.this, "Failed to create directory", Toast.LENGTH_SHORT).show();
						}
					}
				}
			})
			.setNegativeButton("Cancel", null)
			.show();
	}

	@Override
	public void onBackPressed() 
	{
		if (mCurrentDir != null && mCurrentDir.getParentFile() != null && mCurrentDir.getParentFile().canRead()) 
		{
			goUp();
		} 
		else 
		{
			super.onBackPressed();
		}
	}

	private class FolderAdapter extends BaseAdapter 
	{
		@Override
		public int getCount() 
		{
			return mItems.size();
		}

		@Override
		public Object getItem(int position) 
		{
			return mItems.get(position);
		}

		@Override
		public long getItemId(int position) 
		{
			return position;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent) 
		{
			if (convertView == null) 
			{
				convertView = LayoutInflater.from(FolderPickerActivity.this).inflate(R.layout.item_folder, parent, false);
			}

			FileItem item = mItems.get(position);

			TextView tvIcon = convertView.findViewById(R.id.item_icon);
			TextView tvName = convertView.findViewById(R.id.item_name);
			TextView tvDetails = convertView.findViewById(R.id.item_details);
			TextView tvBadge = convertView.findViewById(R.id.item_badge);

			if (item.isParent) 
			{
				tvIcon.setText("⬆");
				tvIcon.setTextColor(0xFFFFAA00);
				tvName.setText(item.name);
				tvDetails.setText("Tap to go to parent folder");
				tvBadge.setVisibility(View.GONE);
			} 
			else 
			{
				tvIcon.setText("📁");
				tvIcon.setTextColor(0xFFE5A00D);
				tvName.setText(item.name);
				tvDetails.setText(item.childCount + " items");

				if (item.hasCstrike) 
				{
					tvBadge.setVisibility(View.VISIBLE);
					tvBadge.setText("★ CS 1.6 DATA");
				} 
				else 
				{
					tvBadge.setVisibility(View.GONE);
				}
			}

			return convertView;
		}
	}
}
