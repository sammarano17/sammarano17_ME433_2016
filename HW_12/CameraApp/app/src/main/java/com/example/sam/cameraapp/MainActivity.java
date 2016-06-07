package com.example.sam.cameraapp;

// libraries
import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.view.WindowManager;
import android.widget.TextView;
import java.io.IOException;
import static android.graphics.Color.blue;
import static android.graphics.Color.green;
import static android.graphics.Color.red;
import static android.graphics.Color.rgb;

public class MainActivity extends Activity implements TextureView.SurfaceTextureListener {
    private Camera mCamera;
    private TextureView mTextureView;
    private SurfaceView mSurfaceView;
    private SurfaceHolder mSurfaceHolder;
    private Bitmap bmp = Bitmap.createBitmap(640,480,Bitmap.Config.ARGB_8888);
    private Canvas canvas = new Canvas(bmp);
    private Paint paint1 = new Paint();
    private TextView mTextView;
    SeekBar myControl;
    SeekBar myControl2;
    TextView myTextView;
    TextView myTextView2;

    static long prevtime = 0; // for FPS calculation

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON); // keeps the screen from turning off

        myControl = (SeekBar) findViewById(R.id.seek1);
        myControl2 = (SeekBar) findViewById(R.id.seek2);
        setMyControlListener();
        setMyControlListener2();
        myTextView = (TextView) findViewById(R.id.textView01);
        myTextView.setText("Enter whatever you Like!");

        myTextView2 = (TextView) findViewById(R.id.textView02);
        myTextView2.setText("Enter number!");

        mSurfaceView = (SurfaceView) findViewById(R.id.surfaceview);
        mSurfaceHolder = mSurfaceView.getHolder();

        mTextureView = (TextureView) findViewById(R.id.textureview);
        mTextureView.setSurfaceTextureListener(this);

        mTextView = (TextView) findViewById(R.id.cameraStatus);

        paint1.setColor(0xffff0000); // red
        paint1.setTextSize(24);
    }

    private void setMyControlListener() {
        myControl.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {

            int progressChanged = 0;

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                myTextView.setText("The value is: "+progress);
                progressChanged = progress;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });
    }

    private void setMyControlListener2() {
        myControl2.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {

            int progressChanged = 0;

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                myTextView2.setText("The value is: "+progress);
                progressChanged = progress;
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });
    }

    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
        mCamera = Camera.open();
        Camera.Parameters parameters = mCamera.getParameters();
        parameters.setPreviewSize(640, 480);
        parameters.setColorEffect(Camera.Parameters.EFFECT_NONE); // black and white
        parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_INFINITY); // no autofocusing
        parameters.setFlashMode(Camera.Parameters.FLASH_MODE_TORCH);
        mCamera.setParameters(parameters);
        mCamera.setDisplayOrientation(90); // rotate to portrait mode


        try {
            mCamera.setPreviewTexture(surface);
            mCamera.startPreview();
        } catch (IOException ioe) {
            // Something bad happened
        }
    }

    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {
        // Ignored, Camera does all the work for us
    }

    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        mCamera.stopPreview();
        mCamera.release();
        return true;
    }

    // the important function
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
        // Invoked every time there's a new Camera preview frame
        mTextureView.getBitmap(bmp);

        final Canvas c = mSurfaceHolder.lockCanvas();
        if (c != null) {

            int[] pixels = new int[bmp.getWidth()];
            //int[] pixels2 = new int[bmp.getWidth()];
            int startY = 300; // which row in the bitmap to analyse to read
            //int startY2 = 400;
            //int Ytest = 100;
            // only look at one row in the image
            //bmp.getPixels(pixels, 0, bmp.getWidth(), 0, startY, bmp.getWidth(), 1); // (array name, offset inside array, stride (size of row), start x, start y, num pixels to read per row, num rows to read)
            //bmp.getPixels(pixels2, 0, bmp.getWidth(), 0, startY2, bmp.getWidth(), 1); // (array name, offset inside array, stride (size of row), start x, start y, num pixels to read per row, num rows to read)

            // pixels[] is the RGBA data (in black an white).
            // instead of doing center of mass on it, decide if each pixel is dark enough to consider black or white
            // then do a center of mass on the thresholded array
            int[] thresholdedPixels = new int[bmp.getWidth()];
            int[] thresholdedColors = new int[bmp.getWidth()];
            //int[] thresholdedPixels2 = new int[bmp.getWidth()];
            int threshold = (int)(myControl.getProgress()*255/100);
            int wbTotal = 0; // total mass
            //int wbTotal2 = 0; // total mass
            int wbCOM = 0; // total (mass time position)
            //int wbCOM2 = 0; // total (mass time position)
            int COM2 = 0;

            for(int jj=0;jj<100;jj++) {

                bmp.getPixels(pixels, 0, bmp.getWidth(), 0, startY, bmp.getWidth(), 1);
                wbTotal = 0;
                wbCOM = 0;

                for (int i = 0; i < bmp.getWidth(); i++) {
                    // sum the red, green and blue, subtract from 255 to get the darkness of the pixel.
                    // if it is greater than some value (600 here), consider it black
                    // play with the 600 value if you are having issues reliably seeing the line

                /*Black and White code*/
                    //if (255*3-(red(pixels[i])+green(pixels[i])+blue(pixels[i])) > 400) {
                    //    thresholdedPixels[i] = 255*3;
                    //}
                    if (255 - (green(pixels[i]) - red(pixels[i])) > threshold) {
                        thresholdedPixels[i] = 255;
                        thresholdedColors[i] = rgb(0, 0, 0);
                    } else {
                        thresholdedPixels[i] = 0;
                        thresholdedColors[i] = rgb(0, 255, 0);
                    }
                    wbTotal = wbTotal + thresholdedPixels[i];
                    wbCOM = wbCOM + thresholdedPixels[i] * i;

                }
                if (jj == 50) {
                    if (wbTotal <= 0) {
                        COM2 = bmp.getWidth() / 2;
                    } else {
                        COM2 = wbCOM / wbTotal;
                    }
                    canvas.drawCircle(COM2, startY, 5, paint1);
                }
                bmp.setPixels(thresholdedColors, 0, bmp.getWidth(), 0, startY, bmp.getWidth(), 1);
                startY = startY + 1;
            }

//                if (red(pixels2[i])>threshold){
//                    thresholdedPixels2[i] = 255*3;
//                }
//                else {
//                    thresholdedPixels2[i] = 0;
//                }
//                wbTotal2 = wbTotal2 + thresholdedPixels2[i];
//                wbCOM2 = wbCOM2 + thresholdedPixels2[i]*i;
//            }



            int COM1;
            //watch out for divide by 0
            if (wbTotal<=0) {
                COM1 = bmp.getWidth()/2;
            }
            else {
                COM1 = wbCOM/wbTotal;
            }
//            if (wbTotal2<=0) {
//                COM2 = bmp.getWidth()/2;
//            }
//            else {
//                COM2 = wbCOM2/wbTotal2;
//            }
            int Diff, Center;
            int Proportion = 1000;
            Diff = COM2 - COM1;
            Center = (COM2 + COM1)/2;

            Proportion = Proportion + ((Center-320));
            Proportion = Proportion + (Diff);

            // draw a circle where you think the COM is
            canvas.drawCircle(COM1, startY, 5, paint1);
            // draw a circle where you think the COM is
            //canvas.drawCircle(COM2, startY2, 5, paint1);
            // also write the value as text
            canvas.drawText("COM1 = " + COM1, 10, 200, paint1);
            //c.drawBitmap(bmp, 0, 0, null);
            //mSurfaceHolder.unlockCanvasAndPost(c);

            canvas.drawText("COM2 = " + COM2, 10, 220, paint1);
            canvas.drawText("Diff = " + Diff, 10, 240, paint1);
            canvas.drawText("Center = " + Center, 10, 260, paint1);
            canvas.drawText("Proportion = " + Proportion, 10, 280, paint1);
            c.drawBitmap(bmp, 0, 0, null);
            mSurfaceHolder.unlockCanvasAndPost(c);

            // calculate the FPS to see how fast the code is running
            long nowtime = System.currentTimeMillis();
            long diff = nowtime - prevtime;
            mTextView.setText("FPS " + 1000/diff);
            prevtime = nowtime;
        }
    }
}

