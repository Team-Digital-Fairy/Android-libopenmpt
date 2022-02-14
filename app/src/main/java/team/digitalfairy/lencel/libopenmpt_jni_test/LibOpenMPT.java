package team.digitalfairy.lencel.libopenmpt_jni_test;

public class LibOpenMPT {
    // JNI Test Hello World function
    public static native void HelloWorld();

    // get OpenMPT String
    public static native String getOpenMPTString(String key);

    // Open_Probability Call
    public static native double OpenProbability(String filename, double effort);

    public static native int loadFile(String filename);

    public static native void OpenSLES_Test();
}