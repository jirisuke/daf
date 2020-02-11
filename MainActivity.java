/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.sample.echo;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.os.Bundle;
import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;


import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.util.Locale;

public class MainActivity extends Activity
        implements ActivityCompat.OnRequestPermissionsResultCallback {
    private static final int AUDIO_ECHO_REQUEST = 0;

    private Button   controlButton;
    private TextView statusView;
    private String  nativeSampleRate;
    private String  nativeSampleBufSize;

    private SeekBar delaySeekBar_L;
    private TextView curDelayTV_L;                                                                    //遅延表示されるもの？
    private int echoDelayProgress_L;                                                                  //遅延時間として渡される？

    private SeekBar delaySeekBar_R;
    private TextView curDelayTV_R;
    private int echoDelayProgress_R;

    private boolean supportRecording;
    private Boolean isPlaying = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main3);

        controlButton = (Button)findViewById((R.id.switch1));
        statusView = (TextView)findViewById(R.id.statusView);
        queryNativeAudioParameters();

        delaySeekBar_L = (SeekBar)findViewById(R.id.delaySeekBar_L);                                      //左
        curDelayTV_L = (TextView)findViewById(R.id.curDelay_L);
        delaySeekBar_L.setMax(200);                                                                      //id curdelay位置不明　とりあえずそのまま
        delaySeekBar_L.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override                                                                              //68から
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                String str = String.format(Locale.US, "%d [ms]", progress);              //Stringクラスのインスタンスstrのメソッドformatからバーの位置情報算出。
                curDelayTV_L.setText(str);                                                        //算出した位置情報をmainクラスの遅延時間として表示する。
                echoDelayProgress_L = progress;                                                     // progressがdelaySeekBar_L.getMax()と同じならechoDelayProgress_Lは1000になる。最大は1秒＝1000ms　単位はms
                if (echoDelayProgress_L == 0) echoDelayProgress_L = 4;                             //48khz/バッファ192Sample=4ms <= bufSizeL_ || bufSizeR_ 　で遅延用バッファの処理がキャンセルされるから0s設定時には最小値4msで対応させる。
                configureEcho(echoDelayProgress_L,echoDelayProgress_R);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });


        delaySeekBar_R = (SeekBar)findViewById(R.id.delaySeekBar_R);
        curDelayTV_R = (TextView)findViewById(R.id.curDelay_R);
        delaySeekBar_R.setMax(200);
        delaySeekBar_R.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                String str = String.format(Locale.US, "%d [ms]", progress);              //Stringクラスのインスタンスstrのメソッドformatからバーの位置情報算出。
                curDelayTV_R.setText(str);                                                        //算出した位置情報をmainクラスの遅延時間として表示する。
                echoDelayProgress_R = progress;
                if (echoDelayProgress_R == 0) echoDelayProgress_R = 4;                                         // ディレイ用のバッファサイズはframesPerBuf　よりも大きくないといけない。　とりあえず最小値を10msと指定しておく。本当はframesPerBufを設定したほうがよい。
                configureEcho(echoDelayProgress_L,echoDelayProgress_R);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });



                                                                                                        // decaySeekBar = (SeekBar)findViewById(R.id.decaySeekBar);
                                                                                                        //curDecayTV = (TextView)findViewById(R.id.curDecay);
                                                                                                        //echoDecayProgress = (float)decaySeekBar.getProgress() / decaySeekBar.getMax();

        // initialize native audio system
        updateNativeAudioUI();

        if (supportRecording) {                                                                         //どこでsupportRecording真偽値ONにしてる？
            createSLEngine(
                    Integer.parseInt(nativeSampleRate),
                    Integer.parseInt(nativeSampleBufSize),
                    echoDelayProgress_L,
                    echoDelayProgress_R                                                            //audio_mainで、delayInMmとおく
                    );                                                                                      //echoDecayProgress
        }
    }



    @Override
    protected void onDestroy() {
        if (supportRecording) {
            if (isPlaying) {
                stopPlay();
            }
            deleteSLEngine();
            isPlaying = false;
        }
        super.onDestroy();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    private void startEcho() {
        if(!supportRecording){
            return;
        }
        if (!isPlaying) {
            if(!createSLBufferQueueAudioPlayer()) {
                statusView.setText(getString(R.string.player_error_msg));
                return;
            }
            if(!createAudioRecorder()) {
                deleteSLBufferQueueAudioPlayer();
                statusView.setText(getString(R.string.recorder_error_msg));
                return;
            }
            startPlay();                                                                            // startPlay() triggers startRecording()   ここ？？？？
            statusView.setText(getString(R.string.echoing_status_msg));
        } else {
            stopPlay();  // stopPlay() triggers stopRecording()
            updateNativeAudioUI();
            deleteAudioRecorder();
            deleteSLBufferQueueAudioPlayer();
        }
        isPlaying = !isPlaying;
        controlButton.setText(getString(isPlaying ?
                R.string.cmd_stop_delay: R.string.cmd_start_delay));
    }
    public void onEchoClick(View view) {
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) !=
                PackageManager.PERMISSION_GRANTED) {
            statusView.setText(getString(R.string.request_permission_status_msg));
            ActivityCompat.requestPermissions(
                    this,
                    new String[] { Manifest.permission.RECORD_AUDIO },
                    AUDIO_ECHO_REQUEST);
            return;
        }
        startEcho();
    }

    public void getLowLatencyParameters(View view) {
        updateNativeAudioUI();
    }

    private void queryNativeAudioParameters() {
        supportRecording = true;
        AudioManager myAudioMgr = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if(myAudioMgr == null) {
            supportRecording = false;
            return;
        }
        nativeSampleRate  =  myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        nativeSampleBufSize =myAudioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);

        // hardcoded channel to mono: both sides -- C++ and Java sides
        int recBufSize = AudioRecord.getMinBufferSize(                                               //バッファサイズ決定
                Integer.parseInt(nativeSampleRate),
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT);
        if (recBufSize == AudioRecord.ERROR ||
                recBufSize == AudioRecord.ERROR_BAD_VALUE) {
            supportRecording = false;
        }

    }
    private void updateNativeAudioUI() {
        if (!supportRecording) {                                                                    //supportRecordingがfalse時、エラーメッセージ
            statusView.setText(getString(R.string.mic_error_msg));
            controlButton.setEnabled(false);
            return;
        }

        statusView.setText(getString(R.string.fast_audio_info_msg,                                   //現在のサンプリング周波数、バッファサイズに表示
                nativeSampleRate, nativeSampleBufSize));
    }
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {
        /*
         * if any permission failed, the sample could not play
         */
        if (AUDIO_ECHO_REQUEST != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        if (grantResults.length != 1  ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED) {
            /*
             * When user denied permission, throw a Toast to prompt that RECORD_AUDIO
             * is necessary; also display the status on UI
             * Then application goes back to the original state: it behaves as if the button
             * was not clicked. The assumption is that user will re-click the "start" button
             * (to retry), or shutdown the app in normal way.
             */
            statusView.setText(getString(R.string.permission_error_msg));
            Toast.makeText(getApplicationContext(),
                    getString(R.string.permission_prompt_msg),
                    Toast.LENGTH_SHORT).show();
            return;
        }

        /*
         * When permissions are granted, we prompt the user the status. User would
         * re-try the "start" button to perform the normal operation. This saves us the extra
         * logic in code for async processing of the button listener.
         */
        statusView.setText(getString(R.string.permission_granted_msg,getString(R.string.cmd_start_delay)));


        // The callback runs on app's thread, so we are safe to resume the action
        startEcho();
    }

    /*
     * Loading our lib
     */
    static {
        System.loadLibrary("echo");
    }

    /*
     * jni function declarations
     */
    static native void createSLEngine(int rate, int framesPerBuf,
                                      long delayRInMs,long delayLInMs);                                              //, float decay
    static native void deleteSLEngine();
    static native boolean configureEcho(int delayLInMs,int delayRInMs);                                             //バーの位置echoDelayProgressを受け取り真偽値返す
    static native boolean createSLBufferQueueAudioPlayer();
    static native void deleteSLBufferQueueAudioPlayer();

    static native boolean createAudioRecorder();
    static native void deleteAudioRecorder();
    static native void startPlay();
    static native void stopPlay();
}
//, echoDecayProgress