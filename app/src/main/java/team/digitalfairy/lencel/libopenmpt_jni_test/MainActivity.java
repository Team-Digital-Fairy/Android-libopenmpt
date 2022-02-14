package team.digitalfairy.lencel.libopenmpt_jni_test;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.media.session.MediaSessionCompat;
import android.util.Log;
import android.widget.TextView;

import permissions.dispatcher.NeedsPermission;
import permissions.dispatcher.RuntimePermissions;

import static team.digitalfairy.lencel.libopenmpt_jni_test.LibOpenMPT.*;

/*
    MANAGE_STORAGEはユーザーが明示的に設定する必要あり。
    Intentを呼べばいいはず？

 */
@RuntimePermissions
public class MainActivity extends AppCompatActivity {
    private static final String MAINACTIVITY_LOGTAG = "MainAct_Log";
    private static double probability = 0.0;

    private static String FILE_NAME = "/sdcard/mod/Seomadan_Uplink_SampleChange1.xm";


    private MediaSessionCompat mediaSession;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        System.loadLibrary("openmpt");
        Log.i(MAINACTIVITY_LOGTAG, "System Loaded Library.");

        //mediaSession = new MediaSessionCompat(this, MAINACTIVITY_LOGTAG);
        HelloWorld();
        OpenSLES_Test();

        TextView tv = findViewById(R.id.textView);
        tv.append(": "+getOpenMPTString("core_version"));

        getHowMuchProbability(FILE_NAME,1.0);

        Log.d(MAINACTIVITY_LOGTAG, String.valueOf(probability));

        if(probability > 0.5) {
            loadFile(FILE_NAME);
        }

    }

    @NeedsPermission({Manifest.permission.MANAGE_EXTERNAL_STORAGE, Manifest.permission.WRITE_EXTERNAL_STORAGE, Manifest.permission.READ_EXTERNAL_STORAGE})
    void getHowMuchProbability(String filename, double effort) {
        probability = OpenProbability(filename, effort);
    }
}