package team.digitalfairy.lencel.libopenmpt_jni_test;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.media.session.MediaSessionCompat;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import static team.digitalfairy.lencel.libopenmpt_jni_test.LibOpenMPT.*;

import org.w3c.dom.Text;

import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

/*
   Copyright 2022 Chromaryu <knight.ryu12@gmail.com>
   Copyright 2022 Vitaly Novichkov <admin@wohlnet.ru>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */


/*
    MANAGE_STORAGEはユーザーが明示的に設定する必要あり。
    Intentを呼べばいいはず？

 */

public class MainActivity extends AppCompatActivity {
    private final ScheduledExecutorService ex = Executors.newScheduledThreadPool(1);
    private ScheduledFuture<?> sf = null;
    private static final String MAINACTIVITY_LOGTAG = "MainAct_Log";
    private static double probability = 0.0;

    // private static String FILE_NAME = "/sdcard/mod/Seomadan_Uplink_SampleChange1.xm";

    public static final int READ_PERMISSION_FOR_MUSIC = 2;

    private String              m_lastPath = "";

    private MediaSessionCompat mediaSession;

    TextView status_d;
    TextView mod_title;
    LinearLayout ll;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        status_d = findViewById(R.id.status_mod);
        ll = findViewById(R.id.layoutLinear);


        if(android.os.Build.VERSION.SDK_INT <= android.os.Build.VERSION_CODES.P)
            m_lastPath = Environment.getExternalStorageDirectory().getPath();
        else
            m_lastPath = "/storage/emulated/0";

        System.loadLibrary("openmpt");
        Log.i(MAINACTIVITY_LOGTAG, "System Loaded Library.");

        AudioManager am = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        Log.i(MAINACTIVITY_LOGTAG,"NSR:"+am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)+" FPB:"+am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER));

        //mediaSession = new MediaSessionCompat(this, MAINACTIVITY_LOGTAG);
        HelloWorld();
        openOpenSLES(Integer.parseInt(am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)), Integer.parseInt(am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)));

        TextView tv = findViewById(R.id.textView);
        tv.append(": " + getOpenMPTString("core_version"));
        mod_title = findViewById(R.id.modtitle);
        RadioGroup rg = findViewById(R.id.interp_group);
        rg.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                RadioButton rb = findViewById(checkedId);
                int r = Integer.parseInt((String) rb.getTag());
                Log.d(MAINACTIVITY_LOGTAG,"change interp to "+r);
                setRenderParam(3,r);
            }
        });

        Button openfb = (Button) findViewById(R.id.button);
        openfb.setOnClickListener(new View.OnClickListener()
        {
            @Override
            public void onClick(View view) {
                OnOpenFileClick(view);
            }
        });
    }



    public void OnOpenFileClick(View view) {
        // Here, thisActivity is the current activity
        if(checkFilePermissions(READ_PERMISSION_FOR_MUSIC))
            return;
        openMusicFileDialog();
    }

    private boolean checkFilePermissions(int requestCode)
    {
        final int grant = PackageManager.PERMISSION_GRANTED;


        final String exStorage = Manifest.permission.READ_EXTERNAL_STORAGE;
        if (ContextCompat.checkSelfPermission(this, exStorage) == grant) {
            Log.d(MAINACTIVITY_LOGTAG, "File permission is granted");
        } else {
            Log.d(MAINACTIVITY_LOGTAG, "File permission is revoked");
        }


        if((ContextCompat.checkSelfPermission(this, exStorage) == grant))
            return false;

        // Should we show an explanation?
        if(ActivityCompat.shouldShowRequestPermissionRationale(this, exStorage))
        {
            // Show an explanation to the user *asynchronously* -- don't block
            // this thread waiting for the user's response! After the user
            // sees the explanation, try again to request the permission.
            AlertDialog.Builder b = new AlertDialog.Builder(this);
            b.setTitle("Permission denied");
            b.setMessage("Sorry, but permission is denied!\n"+
                    "Please, check the Read Extrnal Storage permission to application!");
            b.setNegativeButton(android.R.string.ok, null);
            b.show();
            return true;
        }
        else
        {
            // No explanation needed, we can request the permission.
            ActivityCompat.requestPermissions(this, new String[] { exStorage }, requestCode);
            // MY_PERMISSIONS_REQUEST_READ_EXTERNAL_STORAGE
            // MY_PERMISSIONS_REQUEST_READ_EXTERNAL_STORAGE is an
            // app-defined int constant. The callback method gets the
            // result of the request.
        }
        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (grantResults.length > 0 &&
                permissions[0].equals(Manifest.permission.READ_EXTERNAL_STORAGE) &&
                grantResults[0] == PackageManager.PERMISSION_GRANTED
        ) {
            if (requestCode == READ_PERMISSION_FOR_MUSIC) {
                openMusicFileDialog();
            }
        }
    }

    public void openMusicFileDialog()
    {
        OpenFileDialog fileDialog = new OpenFileDialog(this)
                .setFilter(".*")
                .setCurrentDirectory(m_lastPath)
                .setOpenDialogListener((fileName, lastPath) -> {
                    // processMusicFile(fileName, lastPath);
                    m_lastPath = lastPath;
                    if(sf != null) sf.cancel(true);
                    stopPlaying();

                    getHowMuchProbability(fileName,1.0);

                    Log.d(MAINACTIVITY_LOGTAG, String.valueOf(probability));

                    if(probability > 0.5) {
                        loadFile(fileName);
                    }

                    ctlSetRepeat(-1);
                    mod_title.setText(getMetadata("title"));
                    ll.removeAllViews();
                    int channels = getNumChannel();
                    TextView[] tvs = new TextView[channels];
                    for(int i=0; i<channels; i++) {
                        tvs[i] = new TextView(this);
                        //tvs[i].setId(1000+i);
                        //tvs[i].setText("");
                        ll.addView(tvs[i]);
                    }

                    // TODO: This seems to cause crash over time. at random. Figure out why?
                    // TODO: String.format is slow. It may crash, again.

                    sf = ex.scheduleAtFixedRate(() -> {
                        String a = String.format(
                                "Spd:%02d BPM:%3d Pos:%02X Ptn:%02X Ord:%3d",
                                getSpeed(),getTempo(),getRow(),getPattern(),getOrder()
                        );

                        // prepare before update.
                        String sv[] = new String[channels];
                        for(int i=0; i<channels; i++) {
                            sv[i] = String.format("%02d: L:%f R:%f", i, getVULeft(i), getVURight(i));
                        }
                        runOnUiThread(() -> {
                            status_d.setText(a);
                            for (int i = 0; i < channels; i++) {
                                tvs[i].setText(sv[i]);
                            }
                        });
                    },0,38, TimeUnit.MILLISECONDS);


                    togglePause();
                });
        fileDialog.show();
    }



    @Override
    protected void onStop() {
        super.onStop();

        Log.d(MAINACTIVITY_LOGTAG,"Onstop()");

    }

    // @NeedsPermission({Manifest.permission.MANAGE_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE})
    void getHowMuchProbability(String filename, double effort) {
        probability = OpenProbability(filename, effort);
    }
}