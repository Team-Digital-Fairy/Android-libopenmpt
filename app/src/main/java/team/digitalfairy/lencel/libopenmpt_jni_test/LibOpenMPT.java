package team.digitalfairy.lencel.libopenmpt_jni_test;

public class LibOpenMPT {
    // JNI Test Hello World function
    public static native void HelloWorld();

    // get OpenMPT String
    public static native String getOpenMPTString(String key);

    // Open_Probability Call
    public static native double OpenProbability(String filename, double effort);

    public static native int loadFile(String filename);

    public static native void openOpenSLES(int nsr, int fpb);

    public static native void togglePause();

    public static native void stopPlaying();


    public static native void closeOpenSLES();


    // Controls
    public static native String getMetadata(String key);

    public static native int getNumChannel();
    public static native float getVULeft(int nums);
    public static native float getVURight(int nums);

    public static native int getOrder();
    public static native int getPattern();
    public static native int getRow();
    public static native int getSpeed();
    public static native int getTempo();

    public static native void setRenderParam(int param, int value);
    public static native void ctlSetRepeat(int repeat_count);

    public static native String rRowStrings();
    public static native String rVUStrings(int num);

    public static native double getCurrentTime();
    public static native double getModuleTime();
}
