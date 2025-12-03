package nightradio.androidlib;

import android.Manifest;
import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.ExifInterface;
import android.media.Image;
import android.media.ImageReader;
import android.media.AudioManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Handler;
import android.provider.MediaStore;
import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.provider.Settings;
import android.view.Surface;
import android.view.View;
import android.util.Log;
import android.util.Size;
import android.view.WindowInsets;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.nio.ByteBuffer;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.Enumeration;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class AndroidLib {
    static {
        System.loadLibrary("sundog");
    }

    static class midiCon {
        public int dev = -1; //device index in mdevs[]
        public int port = -1; //port index for openInputPort/openOutputPort
        public int rw = 0; //0 - open for reading; 1 - open for writing;
        public int port_id = 0; //SunDog MIDI port id
        public byte[] wbuf; //writing buffer
        public int wbuf_ptr = 0;
        public MidiOutputPort outputPort; //port for reading
        public MidiInputPort inputPort; //port for writing
    }
    static class midiReceiver extends MidiReceiver {
        private int id = -1; //SunDog MIDI port id
        public midiReceiver( int port_id ) { id = port_id; }
        public void onSend( byte[] data, int offset, int count, long timestamp ) throws IOException {
            //timestamp increases monotonically (always increases)
            //Log.i("MSG", id + " " + String.valueOf(count));
            AndroidLib.midi_receiver_callback( id, data, offset, count, timestamp - System.nanoTime() );
        }
    }
    public static native int midi_receiver_callback( int port_id, byte[] data, int offset, int count, long timestamp );

    private Activity activity;
    private Intent newIntent; //for GetIntentFile(1)
    private String pastedString;
    //MIDI:
    private MidiManager mm;
    private midiCon[] mcons; //MIDI connections
    private Object[] mdevs; //open MIDI devices; MidiDevice[] gives CodeVerify error "aget-object on non-array class" on OS 4.4
    //Other:
    private BufferedInputStream mZipInputStream;
    private String mLocation;
    private byte[] internalHash = new byte[128];
    private int internalHashLen;
    private int sdk;

    public AndroidLib(Activity act) {
        activity = act;
        mm = null;
        sdk = android.os.Build.VERSION.SDK_INT;
    }

    private int permResult = -1;
    String bit2perm( int bitnum )
    {
        String perm = null;
        switch( bitnum )
        {
            case 0: perm = Manifest.permission.WRITE_EXTERNAL_STORAGE; break;
            case 1: perm = Manifest.permission.RECORD_AUDIO; break;
            case 2: perm = Manifest.permission.CAMERA; break;
            case 3: perm = Manifest.permission.READ_EXTERNAL_STORAGE; break;
            case 4: perm = Manifest.permission.READ_MEDIA_AUDIO; break;
            case 5: perm = Manifest.permission.READ_MEDIA_IMAGES; break;
            case 6: perm = Manifest.permission.READ_MEDIA_VIDEO; break;
            case 7: perm = Manifest.permission.MANAGE_EXTERNAL_STORAGE; break;
        }
        return perm;
    }
    int AllFilesAccess( int flags ) //show screen for controlling if the app can manage external storage (broad access)
    {
        if( sdk >= 30 ) {
            //Android 11+; API LEVEL 30+
            //if (Environment.isExternalStorageManager()) {}
            Context ctx = activity.getApplicationContext();
            Intent intent = new Intent();
            intent.setAction(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            Uri uri = Uri.fromParts("package", ctx.getPackageName(), null);
            intent.setData(uri);
            activity.startActivity(intent); //required: ALLOW_WORK_IN_BG (native code);
            //activity.startActivityForResult(intent,1000); //required: ALLOW_WORK_IN_BG (native code) + onActivityResult (MyNativeActivity)
        }
        return 0;
    }
    //p (requested permissions): -1 - get the req. answer; 1<<0 - write ext.storage; 1<<1 - record_audio; 1<<2 - camera; ...
    //retval: enabled permissions;
    public int CheckForPermissions( int p )
    {
        int rv = 0;
        if( sdk < 23 ) return 0xFFFF; // Android 6 and lower
        if( p == -1 ) {
            return permResult;
        } else {
            p &= ~(1<<7); //ignore MANAGE_EXTERNAL_STORAGE
            int reqcnt = 0;
            int reqbits = 0;
            for( int bit = 0; bit < 16; bit++ )
            {
                if( ( p & (1<<bit) ) != 0 )
                {
                    String perm = bit2perm(bit);
                    if( perm == null ) continue;
                    if( ContextCompat.checkSelfPermission(activity,perm) == PackageManager.PERMISSION_GRANTED ) {
                        rv |= (1 << bit);
                    }
                    else {
                        reqbits |= (1<<bit);
                        reqcnt++;
                    }
                }
            }
            if( rv != p && reqcnt > 0 )
            {
                permResult = -1;
                String[] perms = new String[ reqcnt ];
                for( int bit = 0, i = 0; bit < 16; bit++ ) {
                    if( ( reqbits & (1<<bit) ) != 0 ) {
                        perms[ i ] = bit2perm(bit);
                        i++;
                    }
                }
                ActivityCompat.requestPermissions(activity,perms,12345);
            }
            else
            {
                permResult = p;
            }
        }
        return rv;
    }
    public void onRequestPermissionsResult( int requestCode, String[] permissions, int[] grantResults )
    {
        if( requestCode == 12345 )
        {
            int res = 0;
            for( int i = 0; i < grantResults.length; i++ )
            {
                if( grantResults[i] != PackageManager.PERMISSION_GRANTED ) continue;
                if( permissions[i].compareTo(Manifest.permission.WRITE_EXTERNAL_STORAGE) == 0 ) { res |= 1<<0; continue; }
                if( permissions[i].compareTo(Manifest.permission.RECORD_AUDIO) == 0 ) { res |= 1<<1; continue; }
                if( permissions[i].compareTo(Manifest.permission.CAMERA) == 0 ) { res |= 1<<2; continue; }
                if( permissions[i].compareTo(Manifest.permission.READ_EXTERNAL_STORAGE) == 0 ) { res |= 1<<3; continue; }
                if( sdk >= 33 ) {
                    if (permissions[i].compareTo(Manifest.permission.READ_MEDIA_AUDIO) == 0) { res |= 1 << 4; continue; }
                    if (permissions[i].compareTo(Manifest.permission.READ_MEDIA_IMAGES) == 0) { res |= 1 << 5; continue; }
                    if (permissions[i].compareTo(Manifest.permission.READ_MEDIA_VIDEO) == 0) { res |= 1 << 6; continue; }
                }
            }
            permResult = res;
        }
    }
    //Get list of all permissions from the AndroidManifest.xml:
    public String GetRequestedPermissions( int flags )
    {
        Context ctx = activity.getApplicationContext();
        PackageManager pm = ctx.getPackageManager();
        PackageInfo info = null;
        try {
            if( sdk >= 33 ) {
                info = pm.getPackageInfo(ctx.getPackageName(), PackageManager.PackageInfoFlags.of(PackageManager.GET_PERMISSIONS));
            }
            else {
                info = pm.getPackageInfo(ctx.getPackageName(), PackageManager.GET_PERMISSIONS);
            }
        } catch (Exception e) {
            Log.e("GetPermissions()", "getPackageInfo() error", e);
            return null;
        }
        String requested_permissions[] = info.requestedPermissions;
        String rv = "";
        for( int i = 0; i < requested_permissions.length; i++ )
        {
            rv += requested_permissions[ i ];
            rv += " ";
        }
        return rv;
    }

    public String GetDir(String dirID) {
        Context ctx = activity.getApplicationContext();
        if (dirID.equals("internal_cache")) {
            try {
                File f = ctx.getCacheDir();
                return f.toString();
            } catch (Exception e) {
                Log.e("GetDir(internal_cache)", "getCacheDir() error", e);
                return null;
            }
        }
        if (dirID.equals("internal_files")) {
            try {
                File f = ctx.getFilesDir();
                return f.toString();
            } catch (Exception e) {
                Log.e("GetDir(internal_files)", "getFilesDir() error", e);
                return null;
            }
        }
        if (dirID.equals("external_cache")) {
            try {
                File f = ctx.getExternalCacheDir();
                return f.toString();
            } catch (Exception e) {
                Log.e("GetDir(external_cache)", "getFilesDir() error", e);
                return null;
            }
        }
        if (dirID.contains("external_files")) {
            if (dirID.equals("external_files")) {
                //Primary storage device:
                try {
                    File f = ctx.getExternalFilesDir(null);
                    return f.toString();
                } catch (Exception e) {
                    Log.e("GetDir(external_files)", "getExternalFilesDir() error", e);
                    return null;
                }
            }
            if( sdk >= 19 ) { // >= 4.4
                //Another storage devices (secondary SD card, etc.):
                //external_files2, external_files3 ...
                if( dirID.length() >= 15 ) {
                    try {
                        File[] f = ctx.getExternalFilesDirs(null);
                        int n = dirID.charAt(14) - '0'; //external_filesX
                        if (n <= f.length)
                            return f[n - 1].toString();
                        return null;
                    } catch (Exception e) {
                        Log.e("GetDir(external_filesX)", "getExternalFilesDir() error", e);
                        return null;
                    }
                }
            }
        }
        if( sdk < 29 ) { // < 10.0
            if (dirID.equals("external_pictures")) {
                try {
                    File f = Environment.getExternalStorageDirectory();
                    File dcim = new File(f.getAbsolutePath() + "/" + Environment.DIRECTORY_PICTURES);
                    return dcim.toString();
                } catch (Exception e) {
                    Log.e("GetDir(external_pics)", "getExternalFilesDir() error", e);
                    return null;
                }
            }
            if (dirID.equals("external_movies")) {
                try {
                    File f = Environment.getExternalStorageDirectory();
                    File dcim = new File(f.getAbsolutePath() + "/" + Environment.DIRECTORY_MOVIES);
                    return dcim.toString();
                } catch (Exception e) {
                    Log.e("GetDir(external_movies)", "getExternalFilesDir() error", e);
                    return null;
                }
            }
        }
        return null;
    }

    public String GetOSVersion(int par) {
        return android.os.Build.VERSION.RELEASE;
    }

    public String GetLanguage(int par) {
        return Locale.getDefault().toString();
    }

    public String GetHostIPs(int mode) //0 - main IP (Wi-Fi?); 1 - other IPs;
    {
        String rv = "";
        try {
            for( Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces(); interfaces.hasMoreElements(); ) {
                NetworkInterface i = interfaces.nextElement();
                for( Enumeration<InetAddress> addresses = i.getInetAddresses(); addresses.hasMoreElements(); ) {
                    InetAddress a = addresses.nextElement();
                    if( !a.isLoopbackAddress() ) {
                        String addr = a.getHostAddress();
                        if( mode == 1 )
                        {
                            if( rv.length() > 0 ) rv += " ";
                            rv += addr;
                        }
                        if (a instanceof Inet4Address) {
                            if( mode == 0 ) {
                                return addr;
                            }
                        }
                    }
                }
            }
        } catch (Exception ignored) { }
        if( rv.length() == 0 ) rv = null;
        return rv;
    }

    boolean FileExists(String name) {
        File f = new File(name);
        return f.exists();
    }

    boolean HEIC_to_JPEG( ContentResolver resolver, Uri input_uri, String output_path ) {
        Log.i("HEIC_to_JPEG","Converting HEIC -> JPEG...");
        try {
            InputStream input = resolver.openInputStream(input_uri);
            OutputStream out = new FileOutputStream(new File(output_path));
            Bitmap bitmap = BitmapFactory.decodeStream(input);
            if (bitmap != null) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    input.close();
                    input = resolver.openInputStream(input_uri);
                    ExifInterface exif = new ExifInterface(input);
                    int orient = exif.getAttributeInt(ExifInterface.TAG_ORIENTATION, ExifInterface.ORIENTATION_NORMAL);
                    Log.i("HEIC_to_JPEG", "orientation: " + orient);
                    Matrix matrix = new Matrix();
                    boolean changed = true;
                    switch (orient) {
                        case ExifInterface.ORIENTATION_FLIP_HORIZONTAL:
                            matrix.setScale(-1, 1);
                            break;
                        case ExifInterface.ORIENTATION_ROTATE_180:
                            matrix.setRotate(180);
                            break;
                        case ExifInterface.ORIENTATION_FLIP_VERTICAL:
                            matrix.setScale(1, -1);
                            break;
                        case ExifInterface.ORIENTATION_TRANSPOSE:
                            matrix.setRotate(90);
                            matrix.postScale(-1, 1);
                            break;
                        case ExifInterface.ORIENTATION_ROTATE_90:
                            matrix.setRotate(90);
                            break;
                        case ExifInterface.ORIENTATION_TRANSVERSE:
                            matrix.setRotate(-90);
                            matrix.postScale(-1, 1);
                            break;
                        case ExifInterface.ORIENTATION_ROTATE_270:
                            matrix.setRotate(-90);
                            break;
                        default:
                            changed = false;
                            break;
                    }
                    if (changed) {
                        Bitmap rotatedBitmap = Bitmap.createBitmap(bitmap, 0, 0, bitmap.getWidth(), bitmap.getHeight(), matrix, false);
                        if (rotatedBitmap != null) {
                            bitmap.recycle();
                            bitmap = rotatedBitmap;
                        }
                    }
                }
                bitmap.compress(Bitmap.CompressFormat.JPEG, 85, out);
            }
            out.close();
            input.close();
            return true;
        } catch (Exception e) {
            Log.e("HEIC_to_JPEG", "IO Stream exception: " + e.getMessage());
            return false;
        }
    }

    public void SetNewIntent(Intent i) //Set Intent from onNewIntent(), when current activity is already running
    {
        newIntent = i;
    }
    public String GetIntentFile(int par) //Get the file name from some other app
    {
        String rv = null;
        Context ctx = activity.getApplicationContext();
        Intent intent;
        if( par == 0 )
            intent = activity.getIntent();
        else
        {
            intent = newIntent;
            newIntent = null;
        }
        if (intent == null) {
            Log.i("GetIntentFile " + par, "no Intent");
            return null;
        }
        //Log.i("INTENT", "MIME TYPE: " + intent.getType() );
        String action = intent.getAction();
        if (action == null) return null;
        if (action.compareTo(Intent.ACTION_VIEW) == 0 || action.compareTo(Intent.ACTION_SEND) == 0 || action.compareTo(Intent.ACTION_EDIT) == 0) {
            ContentResolver resolver = ctx.getContentResolver();
            Uri uri;
            if (action.compareTo(Intent.ACTION_SEND) == 0) {
                Bundle bundle = intent.getExtras();
                if (bundle == null) return null;
                uri = (Uri) bundle.get(Intent.EXTRA_STREAM);
            } else {
                uri = intent.getData();
                if (uri == null) return null;
            }
            String name = uri.getLastPathSegment();
            if (name == null || name.length() == 0 || name.charAt(0) == ' ') name = "shared file";
            if (name.contains("/"))
            {
                //the name can still have "/" (%2F in uri) - try to read the last path segment again:
                int i = name.lastIndexOf("/");
                if( i >= 0 ) name = name.substring(i + 1);
            }
            rv = GetDir("external_files") + "/" + name;
            if (FileExists(rv)) rv = rv + "_temp";
            String mime_type = resolver.getType(uri);
            Log.v("GetIntentFile", "action: " + action + " : " + intent.getDataString() + " : " + intent.getType());
            Log.v("GetIntentFile", "uri: " + uri + " : " + mime_type);
            Log.v("GetIntentFile", "file name: " + rv);
            try {
                if( mime_type.contains("heic") ) {
                    HEIC_to_JPEG( resolver, uri, rv );
                } else {
                    InputStream input = resolver.openInputStream(uri);
                    OutputStream out = new FileOutputStream(new File(rv));
                    int size = 0;
                    byte[] buffer = new byte[1024 * 4];
                    while ((size = input.read(buffer)) != -1) {
                        out.write(buffer, 0, size);
                    }
                    out.close();
                    input.close();
                }
            } catch (Exception e) {
                Log.e("GetIntentFile", "IO Stream exception: " + e.getMessage());
                rv = null;
            }
        }
        return rv;
    }

    public int OpenURL(String s, int opt) {
        Intent i = new Intent(Intent.ACTION_VIEW);
        i.setData(Uri.parse(s));
        activity.startActivity(i);
        return 0;
    }

    public String getApplicationName() {
        Context ctx = activity.getApplicationContext();
        ApplicationInfo applicationInfo = ctx.getApplicationInfo();
        int stringId = applicationInfo.labelRes;
        return stringId == 0 ? applicationInfo.nonLocalizedLabel.toString() : ctx.getString(stringId);
    }

    public int SendFileToGallery(String path, int opt)
    {
        File file = new File( path );
        Uri fileUri = Uri.fromFile( file );
        String mimeType = "image/jpeg";
        String ext = path.substring( path.lastIndexOf('.') + 1 ).toLowerCase();
        String timeStamp = new SimpleDateFormat("yyyyMMdd_HHmmss").format(new Date());
        String fileName = timeStamp + "." + ext;
        if( ext.equals("png") ) mimeType = "image/png";
        if( ext.equals("gif") ) mimeType = "image/gif";
        if( ext.equals("avi") ) mimeType = "video/avi";
        if( ext.equals("mp4") ) mimeType = "video/mp4";
        int fileCat = 0; //0 - image; 1 - video;
        if( mimeType.contains( "video" ) ) fileCat = 1;
        InputStream inputStream = null;
        OutputStream outputStream = null;
        Uri outputURI = null;
        if( sdk >= 29 ) { // >= 10.0
            Context ctx = activity.getApplicationContext();
            ContentResolver resolver = ctx.getContentResolver();
            ContentValues contentValues = new ContentValues();
            String relPath = null;
            Uri table = null;
            if( fileCat == 0 )
            {
                relPath = Environment.DIRECTORY_PICTURES;
                table = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
            }
            else
            {
                relPath = Environment.DIRECTORY_MOVIES;
                table = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
            }
            contentValues.put( MediaStore.MediaColumns.DISPLAY_NAME, fileName );
            contentValues.put( MediaStore.MediaColumns.MIME_TYPE, mimeType );
            contentValues.put( MediaStore.MediaColumns.RELATIVE_PATH, relPath + "/" + getApplicationName() );
            Uri uri = null;
            try {
                uri = resolver.insert(table, contentValues);
                if( uri != null )
                {
                    inputStream = resolver.openInputStream(fileUri);
                    outputStream = resolver.openOutputStream(uri);
                    int size = 0;
                    byte[] buffer = new byte[1024*4];
                    while ((size = inputStream.read(buffer)) != -1) {
                        outputStream.write(buffer, 0, size);
                    }
                }
            } catch (Exception e) {
                Log.e("SendFileToGallery", "Exception: " + e.getMessage());
                if( uri != null ) resolver.delete(uri, null, null);
            }
        }
        else
        {
            String dir = null;
            if( fileCat == 0 )
            {
                dir = GetDir( "external_pictures" );
            }
            else
            {
                dir = GetDir( "external_movies" );
            }
            dir += "/" + getApplicationName();
            File dirF = new File(dir);
            if (!dirF.exists()) dirF.mkdir();
            File outF = new File( dir, fileName );
            outputURI = Uri.fromFile(outF);
            try {
                inputStream = new FileInputStream(path);
                outputStream = new FileOutputStream(outF);
                int size = 0;
                byte[] buffer = new byte[1024*4];
                while ((size = inputStream.read(buffer)) != -1) {
                    outputStream.write(buffer, 0, size);
                }
            } catch (Exception e) {
                Log.e("SendFileToGallery", "Exception2: " + e.getMessage());
            }
        }
        try {
            if( inputStream != null ) inputStream.close();
            if( outputStream != null ) outputStream.close();
        } catch (Exception e) {
            Log.e("SendFileToGallery", "Exception3: " + e.getMessage());
        }
        if( sdk < 29 ) { // < 10.0
            if( outputURI != null ) {
                Intent scanFileIntent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, outputURI);
                activity.sendBroadcast(scanFileIntent);
            }
        }
        return 0;
    }

    public int ClipboardCopy(final String txt, final int opt) {
        activity.runOnUiThread( new Runnable() {
            @Override
            public void run() {
                Context ctx = activity.getApplicationContext();
                ClipboardManager clipboard = (ClipboardManager)ctx.getSystemService(Context.CLIPBOARD_SERVICE);
                ClipData clip = ClipData.newPlainText("text", txt);
                clipboard.setPrimaryClip(clip);
            } } );
        return 0;
    }

    public String ClipboardPaste(final int opt) {
        final Semaphore ss = new Semaphore(0);
        activity.runOnUiThread( new Runnable() {
            @Override
            public void run() {
                pastedString = null;
                while( true ) {
                    Context ctx = activity.getApplicationContext();
                    ClipboardManager clipboard = (ClipboardManager) ctx.getSystemService(Context.CLIPBOARD_SERVICE);
                    if (!clipboard.hasPrimaryClip()) break;
                    //if (!clipboard.getPrimaryClipDescription().hasMimeType(MIMETYPE_TEXT_PLAIN)) break;
                    ClipData.Item item = clipboard.getPrimaryClip().getItemAt(0);
                    pastedString = item.getText().toString();
                    break;
                }
                ss.release();
            } } );
        try {
            ss.acquire();
        } catch (InterruptedException e) {
            return null;
        }
        return pastedString;
    }

    //
    // Audio
    //

    //Get the number of audio frames that the HAL (Hardware Abstraction Layer) buffer can hold.
    //You should construct your audio buffers so that they contain an exact multiple of this number.
    //If you use the correct number of audio frames, your callbacks occur at regular intervals, which reduces jitter.
    public int GetAudioOutputBufferSize() {
        if (sdk < 17) return 0; // < 4.2
        Context ctx = activity.getApplicationContext();
        AudioManager am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
        String frames = am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
        return Integer.parseInt(frames);
    }

    public int GetAudioOutputSampleRate() {
        if (sdk < 17) return 0; // < 4.2
        Context ctx = activity.getApplicationContext();
        AudioManager am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);
        String rate = am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
        return Integer.parseInt(rate);
    }

    //When enabled, sustained performance mode is intended to provide a consistent level of performance for a prolonged amount of time.
    public int SetSustainedPerformanceMode( final int mode ) {
        if( sdk < 24 ) return -1; // < 7.0
        if( activity.getWindow() == null ) return -1;
        activity.runOnUiThread( new Runnable() {
            @Override
            public void run() {
                boolean enable = mode == 1;
                activity.getWindow().setSustainedPerformanceMode( enable );
            } } );
        return 0;
    }

    //On some devices, the foreground process may have one or more CPU cores exclusively reserved for it.
    //This method can be used to retrieve which cores that are (if any),
    //so the calling process can then use sched_setaffinity() to lock a thread to these cores.
    public int[] GetExclusiveCores( int opt )
    {
        if( sdk < 24 ) return null; // < 7.0
        int[] rv = null;
        try {
            rv = android.os.Process.getExclusiveCores();
            if( rv == null )
                Log.w( "GetExclusiveCores", "NULL :(" );
            else
                Log.i( "GetExclusiveCores", "num. of excl. cores = " + rv.length );
        } catch( RuntimeException e ) {
            Log.w( "GetExclusiveCores", "getExclusiveCores() is not supported on this device");
        }
        return rv;
    }

    //
    // MIDI
    //

    public int MIDIInit() {
        if( sdk < 23 ) return -1; // < 6.0
        if( mm != null ) return 0;
        Context ctx = activity.getApplicationContext();
        if (!ctx.getPackageManager().hasSystemFeature(PackageManager.FEATURE_MIDI)) return -2;
        mm = (MidiManager) ctx.getSystemService(Context.MIDI_SERVICE);
        if( mm == null ) return -3;
        mcons = new midiCon[ 32 ]; //array of null objects
        mdevs = new MidiDevice[ 16 ]; //...
        return 0;
    }

    private String GetMIDIDevportName( String devName, MidiDeviceInfo.PortInfo portInfo )
    {
        String rv = devName;
        if( portInfo.getType() == MidiDeviceInfo.PortInfo.TYPE_INPUT )
            rv += " dst";
        else
            rv += " src";
        rv += String.valueOf( portInfo.getPortNumber() );
        String name = portInfo.getName();
        if( name != null && name.length() > 0 )
            rv += " " + name;
        return rv;
    }

    private String GetMIDIDevName( MidiDeviceInfo dev )
    {
        Bundle devProps = dev.getProperties();
        String name = devProps.getString( MidiDeviceInfo.PROPERTY_NAME );
        //name = "manufacturer product#ID MIDI 1.0",
        //ID may be different after each reconnection of the device,
        //so the user is forced to specify the device again each time :(
        //Use man + prod instead. Disadvantage - we can't work with two identical MIDI devices.
        String man = devProps.getString( MidiDeviceInfo.PROPERTY_MANUFACTURER );
        String prod = devProps.getString( MidiDeviceInfo.PROPERTY_PRODUCT );
        if( man != null && prod != null )
        {
            return man + " " + prod;
        }
        return name;
    }

    //flags: 1 - for reading; 2 - for writing;
    public String GetMIDIDevports( int flags )
    {
        String rv = "";
        if( sdk < 23 ) return null; // < 6.0
        if( mm == null ) return null;
        MidiDeviceInfo[] devs = mm.getDevices();
        for( int i = 0; i < devs.length; i++ )
        {
            MidiDeviceInfo dev = devs[ i ];
            String devName = GetMIDIDevName( dev );
            MidiDeviceInfo.PortInfo[] portInfos = dev.getPorts();
            for( int p = 0; p < portInfos.length; p++ )
            {
                boolean portOk = false;
                if( portInfos[ p ].getType() == MidiDeviceInfo.PortInfo.TYPE_INPUT )
                {
                    //Input port:
                    if( ( flags & 2 ) != 0 ) portOk = true;
                }
                else
                {
                    //Output port:
                    if( ( flags & 1 ) != 0 ) portOk = true;
                }
                if( portOk )
                {
                    rv += GetMIDIDevportName( devName, portInfos[ p ] ) + "\n";
                }
            }
        }
        if( rv.length() <= 1 )
        {
            rv = null;
        }
        return rv; //device+port
    }

    public int OpenMIDIConnection( int conIndex )
    {
        int rv = 0;
        midiCon mc = mcons[ conIndex ];
        MidiDevice device = (MidiDevice)mdevs[ mc.dev ];
        if( mc.rw == 0 )
        {
            //open port for reading:
            try {
                mc.outputPort = device.openOutputPort(mc.port);
                mc.outputPort.connect(new midiReceiver(mc.port_id));
            } catch (Exception e) {
                Log.e("OpenMIDIConnection (r)", "Exception:" + e.getMessage());
                mc.outputPort = null;
                rv = -1;
            }
        }
        else
        {
            //open port for writing:
            try {
                mc.inputPort = device.openInputPort(mc.port);
            } catch (Exception e) {
                Log.e("OpenMIDIConnection (w)", "Exception:" + e.getMessage());
                mc.inputPort = null;
                rv = -1;
            }
        }
        return rv;
    }

    public int OpenMIDIDevport( String devport, final int port_id )
    {
        //Log.i("MIDI","OPEN...");
        int rv = -1;
        if( sdk < 23 ) return -1; // < 6.0
        if( mm == null ) return -1;
        MidiDeviceInfo[] devs = mm.getDevices();
        for( int i = 0; i < devs.length; i++ )
        {
            final MidiDeviceInfo devInfo = devs[ i ];
            String devName = GetMIDIDevName( devInfo );
            MidiDeviceInfo.PortInfo[] portInfos = devInfo.getPorts();
            for( int p = 0; p < portInfos.length; p++ )
            {
                boolean portOk = false;
                if( portInfos[ p ].getType() == MidiDeviceInfo.PortInfo.TYPE_INPUT )
                {
                    //Input port:
                    if( ( port_id & 1 ) == 1 ) portOk = true;
                }
                else
                {
                    //Output port:
                    if( ( port_id & 1 ) == 0 ) portOk = true;
                }
                if( portOk && GetMIDIDevportName( devName, portInfos[ p ] ).equals( devport ) )
                {
                    for( int c = 0; c < mcons.length; c++ )
                    {
                        if( mcons[ c ] != null ) continue;

                        rv = c; //empty slot for the new connection
                        mcons[ c ] = new midiCon();
                        mcons[ c ].port = portInfos[ p ].getPortNumber();
                        mcons[ c ].rw = port_id & 1;
                        mcons[ c ].port_id = port_id;

                        for( int d = 0; d < mdevs.length; d++ )
                        {
                            if( mdevs[ d ] != null )
                            {
                                MidiDevice device = (MidiDevice)mdevs[ d ];
                                if( GetMIDIDevName( device.getInfo() ).equals( devName ) )
                                {
                                    //Device already open:
                                    mcons[ c ].dev = d; //connection <-> device
                                    OpenMIDIConnection( c );
                                    break;
                                }
                            }
                        }
                        if( mcons[ c ].dev >= 0 ) break;

                        //Open device:
                        final Semaphore ss = new Semaphore(0);
                        final int conIndex = c;
                        mm.openDevice( devInfo, new MidiManager.OnDeviceOpenedListener() {
                            @Override
                            public void onDeviceOpened( MidiDevice device ) {
                                if( device == null ) {
                                    Log.e( "open MIDI device", "could not open device " + devInfo );
                                } else {
                                    for( int d = 0; d < mdevs.length; d++ )
                                    {
                                        if( mdevs[ d ] == null )
                                        {
                                            mdevs[ d ] = device;
                                            mcons[ conIndex ].dev = d; //connection <-> device
                                            OpenMIDIConnection( conIndex ); //POSSIBLE CRASH
                                            break;
                                        }
                                    }
                                }
                                ss.release();
                                //Log.i("MIDI","OPEN OK");
                            }
                            }, new Handler( Looper.getMainLooper() ) );

                        try {
                            ss.acquire();
                        } catch (InterruptedException e) {
                            Log.e( "OpenMIDIDevport", "Open Exception", e );
                        }
                        if( mcons[ c ].dev < 0 )
                        {
                            Log.e( "OpenMIDIDevport", "Can't open device" );
                            //Close unused ports:
                            try {
                                if( mcons[ c ].inputPort != null ) mcons[ c ].inputPort.close();
                                if( mcons[ c ].outputPort != null ) mcons[ c ].outputPort.close();
                            } catch( IOException e ) {
                                Log.e( "close MIDI port (2)", "error", e );
                            }
                            //Close connection:
                            mcons[ c ] = null;
                            return -1;
                        }

                        break; //connection is created
                    }
                    break; //port is found
                }
            }
            if( rv >= 0 ) break;
        }
        return rv; //connection index
    }

    public int CloseMIDIDevport( int conIndex )
    {
        //Log.i("MIDI","CLOSE");
        if( sdk < 23 ) return -1; // < 6.0
        if( mm == null ) return -1;
        if( conIndex < 0 || conIndex >= mcons.length ) return -1;

        midiCon mc = mcons[ conIndex ];
        if( mc == null ) return -1;

        //Close unused ports:
        try {
            if( mc.inputPort != null ) mc.inputPort.close();
            if( mc.outputPort != null ) mc.outputPort.close();
        } catch( IOException e ) {
            Log.e( "close MIDI port", "error", e );
        }

        //Close connection:
        mcons[ conIndex ] = null;

        //Close unused devices;
        for( int d = 0; d < mdevs.length; d++ )
        {
            MidiDevice device = (MidiDevice)mdevs[ d ];
            if( device != null )
            {
                boolean deviceUnused = true;
                for( int c = 0; c < mcons.length; c++ )
                {
                    if( mcons[ c ] != null )
                    {
                        if( mcons[ c ].dev == d )
                        {
                            deviceUnused = false;
                            break;
                        }
                    }
                }
                if( deviceUnused )
                {
                    try {
                        device.close();
                    } catch( IOException e ) {
                        Log.e( "close MIDI device", "error", e );
                    }
                    mdevs[ d ] = null;
                }
            }
        }

        return 0;
    }

    public int MIDISend( int conIndex, int msg, int msgLen, int timestamp )
    {
        if (sdk < 23) return -1; // < 6.0
        if (mm == null) return -1;
        if (conIndex < 0 || conIndex >= mcons.length) return -1;

        midiCon mc = mcons[conIndex];
        if( mc == null ) return -1;
        if( mc.inputPort == null ) return -1;

        boolean last_part = true; //last part of long message
        if( msgLen > 4 )
        {
            last_part = false;
            msgLen = 4;
        }

        if( mc.wbuf == null )
        {
            mc.wbuf = new byte[ 256 ];
        }
        for( int i = 0; i < msgLen; i++ )
        {
            mc.wbuf[ mc.wbuf_ptr ] = (byte)( msg & 255 );
            msg >>= 8;
            mc.wbuf_ptr++;
        }
        if( last_part || mc.wbuf_ptr >= mc.wbuf.length - 8 )
        {
            try {
                mc.inputPort.send( mc.wbuf, 0, mc.wbuf_ptr, timestamp + System.nanoTime() );
            } catch( IOException e ) { }
            mc.wbuf_ptr = 0;
        }

        return 0;
    }

    //
    // UI
    //

    public long GetWindowInsets()
    {
        if( sdk < 29 ) return 0; // < 10.0
        if( activity.getWindow() == null ) return 0;
        View decorView = activity.getWindow().getDecorView();
        WindowInsets i = decorView.getRootWindowInsets();
        if( i == null ) return 0;
        long rv = 0;
        rv += (long)i.getSystemWindowInsetLeft();
        rv += (long)i.getSystemWindowInsetTop() << 16;
        rv += (long)i.getSystemWindowInsetRight() << 32;
        rv += (long)i.getSystemWindowInsetBottom() << 48;
        //Log.i( "INSETS", "INSETS: " + decorView.getRootWindowInsets() );
        return rv;
    }

    public int SetSystemUIVisibility( final int uiVisibility )
    {
        if( sdk < 14 ) return -1;
        if( activity.getWindow() == null ) return -1;
        activity.runOnUiThread( new Runnable() {
            @Override
            public void run() {
                View decorView = activity.getWindow().getDecorView();
                int prevFlags = decorView.getSystemUiVisibility();
                int newFlags = prevFlags;
                if( uiVisibility == 0 )
                {
                    //Hide top and bottom bars:
                    if( sdk >= 14 && sdk < 19 ) //4.0 ... 4.3
                    {
                        newFlags = View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_LOW_PROFILE;
                    }
                    else if( sdk >= 19 ) //4.4 KITKAT
                    {
                        newFlags = View.SYSTEM_UI_FLAG_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
                    }
                }
                if( uiVisibility == 1 )
                {
                    //System UI is visible:
                    if( sdk >= 14 && sdk < 19 ) //4.0 ... 4.3
                    {
                        newFlags &= ~View.SYSTEM_UI_FLAG_LOW_PROFILE;
                        newFlags |= View.SYSTEM_UI_FLAG_FULLSCREEN;
                    }
                    else if( sdk >= 19 ) //4.4 KITKAT
                    {
                        newFlags &= ~(View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
                        newFlags |= View.SYSTEM_UI_FLAG_FULLSCREEN;
                    }
                }
                if( newFlags != prevFlags )
                {
                    decorView.setSystemUiVisibility( newFlags );
                }
            } } );
        return 0;
    }

    //
    // Camera
    //

    public static native int camera_frame_callback1( int con_index, long user_data, byte[] data );
    public static native int camera_frame_callback2( int con_index, long user_data, ByteBuffer plane0, ByteBuffer plane1, ByteBuffer plane2, int ps0, int ps1, int rs0, int rs1 );

    class camera1 { //Camera API 1
        private Camera cam;
        private SurfaceTexture st;
        private int mCamIndex; //camera index (for SunDog engine only)
        private int mConIndex; //connection index (for SunDog engine only)
        private long mUserData; //C pointer to SunDog video object
        public int mWidth;
        public int mHeight;

        public int open(int cam_index, int con_index, long user_data) {
            int rv = -1;

            mCamIndex = cam_index;
            mConIndex = con_index;
            mUserData = user_data;

            st = new SurfaceTexture(1024);

            int attempts = 2;
            for (int a = attempts; a > 0; a--) {
                Log.i("Camera1", "Open attempt " + (attempts - a + 1));
                if (cam != null) {
                    //Already opened:
                    return 0;
                }
                try {
                    if (mCamIndex >= Camera.getNumberOfCameras())
                        mCamIndex = 0;
                    cam = Camera.open(mCamIndex);
                } catch (Exception e) {
                    //Camera is not available (in use or does not exist)
                    Log.e("Camera1.open()", "Exception:" + e.getMessage());
                    if (a <= 1)
                        return -1;
                    else {
                        close_and_wait();
                        continue;
                    }
                }
                Camera.Parameters pars = cam.getParameters();
                Camera.Size previewSize = pars.getPreviewSize();
                int max = 1300 * 800;
                mWidth = 0; mHeight = 0;
                if( previewSize.width * previewSize.height > max ) {
                    List<Camera.Size> sizes = pars.getSupportedPreviewSizes();
                    for (Camera.Size s : sizes) {
                        if( s.width * s.height <= max && s.width * s.height > mWidth * mHeight ) {
                            mWidth = s.width;
                            mHeight = s.height;
                        }
                    }
                    if( mWidth > 0 && mHeight > 0 ) pars.setPreviewSize( mWidth, mHeight );
                }
                try {
                    cam.setParameters(pars);
                } catch (Exception e) {
                    Log.e("Camera1.setParameters()", "Exception:" + e.getMessage());
                    if (a <= 1)
                        return -2;
                    else {
                        close_and_wait();
                        continue;
                    }
                }
                previewSize = pars.getPreviewSize();
                mWidth = previewSize.width;
                mHeight = previewSize.height;
                int frame_size = mWidth * mHeight;
                cam.addCallbackBuffer(new byte[frame_size + frame_size / 2]);
                cam.addCallbackBuffer(new byte[frame_size + frame_size / 2]);
                cam.addCallbackBuffer(new byte[frame_size + frame_size / 2]);
                cam.addCallbackBuffer(new byte[frame_size + frame_size / 2]);
                cam.setPreviewCallbackWithBuffer(new Camera.PreviewCallback() {
                    public synchronized void onPreviewFrame(byte[] data, Camera camera) {
                        camera_frame_callback1(mConIndex, mUserData, data);
                        camera.addCallbackBuffer(data);
                    }
                });
                try {
                    cam.setPreviewTexture(st);
                } catch (Exception e) {
                    Log.e("Cam.setPreviewDisplay", "Exception:" + e.getMessage());
                    if (a <= 1)
                        return -3;
                    else {
                        close_and_wait();
                        continue;
                    }
                }
                try {
                    cam.startPreview();
                } catch (Exception e) {
                    Log.e("Camera.startPreview()", "Exception:" + e.getMessage());
                    if (a <= 1)
                        return -4;
                    else {
                        close_and_wait();
                        continue;
                    }
                }
                rv = 0;
                break; //successful
            }

            if (rv == 0)
                Log.i("Camera1", "Opened");
            else
                Log.e("Camera1", "Error " + rv);
            return rv;
        }

        private int close()
        {
            if( cam != null )
            {
                cam.stopPreview();
                cam.setPreviewCallbackWithBuffer(null);
                cam.release();
                cam = null;
                Log.i( "Camera1", "Closed" );
            }
            return 0;
        }

        private void close_and_wait()
        {
            close();
            try {
                Thread.sleep( 100 );
            } catch( Exception ee ) {
                Log.e( "mAllow Sleep", "Exception", ee );
            }
        }

        public int GetCameraFocusMode()
        {
            int rv = 0;
            Camera.Parameters pars = cam.getParameters();
            String mode = pars.getFocusMode();
            if (mode.equals(Camera.Parameters.FOCUS_MODE_AUTO)) rv = 0;
            if (mode.equals(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) rv = 1;
            return rv;
        }

        public int SetCameraFocusMode( int mode_num ) {
            Camera.Parameters pars = cam.getParameters();
            String mode = null;
            switch (mode_num) {
                case 0: mode = Camera.Parameters.FOCUS_MODE_AUTO; break;
                case 1: mode = Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO; break;
            }
            if (mode != null) {
                List<String> focusModes = pars.getSupportedFocusModes();
                if (focusModes.contains(mode)) {
                    pars.setFocusMode(mode);
                    cam.setParameters(pars);
                    if (mode_num == 0) {
                        cam.autoFocus(null);
                    }
                }
            }
            return 0;
        }
    }

    class camera2 { //Camera API 2
        private Semaphore mOpenCloseLock = new Semaphore(1); //to prevent the app from exiting before closing the camera
        private int mCamIndex; //camera index (for SunDog engine only)
        private int mConIndex; //connection index (for SunDog engine only)
        private long mUserData; //C pointer to SunDog video object
        private String mId;
        private CameraDevice mDevice;
        private int mDeviceLevel;
        private HandlerThread mBackgroundThread; //additional thread for running tasks that shouldn't block the UI
        private Handler mBackgroundHandler; //for running tasks in the background
        private ImageReader mImageReader;
        private CaptureRequest.Builder mPreviewRequestBuilder;
        private CaptureRequest mPreviewRequest;
        private CameraCaptureSession mCaptureSession;
        private int[] mAFModes;
        private int mAFMode;
        public int mWidth;
        public int mHeight;
        private int mOrient; //Clockwise angle through which the output image needs to be rotated to be upright on the device screen in its native orientation
        //... use activity.windowManager.defaultDisplay.rotation ?

        //This callback returns an image when CameraCaptureSession completes capture:
        private final ImageReader.OnImageAvailableListener mOnImageAvailableListener = new ImageReader.OnImageAvailableListener() {
            @Override
            public void onImageAvailable(ImageReader reader) {
                Image image = reader.acquireLatestImage();
                if( image != null ) {
                    Image.Plane[] planes = image.getPlanes();
                    if( planes.length >= 3 )
                    {
                        camera_frame_callback2( mConIndex, mUserData,
                                planes[0].getBuffer(), planes[1].getBuffer(), planes[2].getBuffer(),
                                planes[0].getPixelStride(), planes[1].getPixelStride(),
                                planes[0].getRowStride(), planes[1].getRowStride() );
                    }
                    image.close();
                }
            }
        };

        private void SetRepeatingRequest()
        {
            if( mCaptureSession == null ) return;
            try {
                mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_MODE, mAFMode); //autofocus mode
                if( mAFMode == CaptureRequest.CONTROL_AF_MODE_AUTO )
                    mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_TRIGGER, CameraMetadata.CONTROL_AF_TRIGGER_START);
                else
                    mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_TRIGGER, CameraMetadata.CONTROL_AF_TRIGGER_CANCEL);
                mCaptureSession.capture(mPreviewRequestBuilder.build(), null, mBackgroundHandler);
                mPreviewRequestBuilder.set(CaptureRequest.CONTROL_AF_TRIGGER, null);
                mPreviewRequest = mPreviewRequestBuilder.build();
                mCaptureSession.setRepeatingRequest( mPreviewRequest, null, mBackgroundHandler ); //start preview
            } catch (CameraAccessException e) {
                e.printStackTrace();
            } catch (IllegalStateException e) {
                Log.e("SetRepeatingRequest", "IllegalStateException");
            }
        }

        //This is needed to check if the camera session is configured and ready to show a preview:
        private final CameraCaptureSession.StateCallback camSessionStateCallback = new CameraCaptureSession.StateCallback() {
            @Override
            public void onConfigured(CameraCaptureSession session) {
                if( mDevice == null ) return; //the camera is already closed
                //When the session is ready, we start displaying the preview:
                mCaptureSession = session;
                SetRepeatingRequest();
            }
            @Override
            public void onConfigureFailed(CameraCaptureSession session) {}
        };

        //Required to open a camera:
        private final CameraDevice.StateCallback camStateCallback = new CameraDevice.StateCallback() {
            @Override
            public void onOpened(@NonNull CameraDevice cameraDevice) {
                // This method is called when the camera is opened. We start camera preview here.
                mOpenCloseLock.release();
                mDevice = cameraDevice;
                try { //Create preview session:
                    Surface s = mImageReader.getSurface();
                    mPreviewRequestBuilder = mDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                    mPreviewRequestBuilder.addTarget( s );
                    mDevice.createCaptureSession( Arrays.asList(s), camSessionStateCallback, null );
                } catch (CameraAccessException e) {
                    e.printStackTrace();
                } catch (IllegalStateException e) {
                    Log.e("Create preview session", "IllegalStateException");
                }
            }
            @Override
            public void onDisconnected(@NonNull CameraDevice cameraDevice) {
                mOpenCloseLock.release();
                cameraDevice.close();
                mDevice = null;
            }
            @Override
            public void onError(@NonNull CameraDevice cameraDevice, int error) {
                mOpenCloseLock.release();
                cameraDevice.close();
                mDevice = null;
            }
        };

        private boolean isAFModeSupported(int mode)
        {
            for( int m : mAFModes )
            {
                if( m == mode ) return true;
            }
            return false;
        }

        public int open(int cam_index, int con_index, long user_data) {
            int rv = -1;

            mCamIndex = cam_index;
            mConIndex = con_index;
            mUserData = user_data;

            if( mBackgroundThread == null ) {
                mBackgroundThread = new HandlerThread("CameraBackground");
                mBackgroundThread.start();
                mBackgroundHandler = new Handler(mBackgroundThread.getLooper());
            }

            CameraManager cm = (CameraManager) activity.getSystemService(Context.CAMERA_SERVICE);
            int back = -1;
            int front = -1;
            int n = -1;

            try {
                String[] camList = cm.getCameraIdList();
                CameraCharacteristics cc;
                for (int i = 0; i < camList.length; i++) {
                    cc = cm.getCameraCharacteristics(camList[i]);
                    Integer facing = cc.get(CameraCharacteristics.LENS_FACING);
                    if (facing == CameraCharacteristics.LENS_FACING_BACK && back == -1) back = i;
                    if (facing == CameraCharacteristics.LENS_FACING_FRONT && front == -1) front = i;
                }
                if( cam_index == 0 && back != -1 ) n = back;
                if( cam_index == 1 && front != -1 ) n = front;
                if( n == -1 ) n = back;
                if( n == -1 ) n = front;
                if (cam_index >= 2) {
                    int i2 = 2;
                    for (int i = 0; i < camList.length; i++) {
                        if (i == back || i == front) continue;
                        if (i2 == cam_index) {
                            n = i;
                            break;
                        }
                        i2++;
                    }
                }
                if (n == -1) {
                    Log.e("OpenCamera", "No camera with index " + cam_index);
                    return -2;
                }
                mId = camList[n];
                cc = cm.getCameraCharacteristics(mId);
                mOrient = cc.get(CameraCharacteristics.SENSOR_ORIENTATION);
                mDeviceLevel = cc.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL);
                mAFModes = cc.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);
                mAFMode = CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO;
                while( true ) {
                    if (isAFModeSupported(CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO) ) { mAFMode = CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO; break; }
                    if (isAFModeSupported(CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE) ) { mAFMode = CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE; break; }
                    if (isAFModeSupported(CaptureRequest.CONTROL_AF_MODE_AUTO) ) { mAFMode = CaptureRequest.CONTROL_AF_MODE_AUTO; break; }
                    if (isAFModeSupported(CaptureRequest.CONTROL_AF_MODE_EDOF) ) { mAFMode = CaptureRequest.CONTROL_AF_MODE_EDOF; break; }
                    if (isAFModeSupported(CaptureRequest.CONTROL_AF_MODE_OFF) ) { mAFMode = CaptureRequest.CONTROL_AF_MODE_OFF; break; }
                    break;
                }
                StreamConfigurationMap map = cc.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                Size[] sizes = map.getOutputSizes(ImageFormat.YUV_420_888);
                int max = 1300 * 800;
                if( mDeviceLevel == CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY )
                    max = 640 * 480;
                mWidth = 0; mHeight = 0;
                for (Size s : sizes) {
                    //Log.i( "SIZE", s.toString() );
                    if( s.getWidth() * s.getHeight() <= max && s.getWidth() * s.getHeight() > mWidth * mHeight ) {
                        mWidth = s.getWidth();
                        mHeight = s.getHeight();
                    }
                }
                mImageReader = ImageReader.newInstance( mWidth, mHeight, ImageFormat.YUV_420_888, 2 );
                mImageReader.setOnImageAvailableListener( mOnImageAvailableListener, mBackgroundHandler );
                rv = 0;
            } catch (CameraAccessException e) {
                e.printStackTrace();
            } catch (NullPointerException e) {
                Log.e("OpenCamera", "NPE");
            }

            if( rv == 0 ) {
                try {
                    if (!mOpenCloseLock.tryAcquire(2500, TimeUnit.MILLISECONDS)) {
                        throw new RuntimeException("Time out waiting to lock camera opening.");
                    }
                    cm.openCamera(mId, camStateCallback, mBackgroundHandler);
                } catch (CameraAccessException e) {
                    rv = -3;
                    e.printStackTrace();
                } catch (InterruptedException e) {
                    rv = -4;
                    Log.e("OpenCamera", "InterruptedException");
                } catch (SecurityException e) {
                    rv = -5;
                    Log.e("OpenCamera", "SecurityException");
                }
                if( rv != 0 ) mOpenCloseLock.release();
            }

            if( rv == 0 ) {
                Log.i("OpenCamera", "Opened");
            }
            else
                Log.e("OpenCamera", "Error " + rv);
            return rv;
        }

        public int close() {
            try {
                mOpenCloseLock.acquire();
                if (mCaptureSession != null) {
                    mCaptureSession.close();
                    mCaptureSession = null;
                }
                if (mDevice != null) {
                    mDevice.close();
                    mDevice = null;
                }
                if (mImageReader != null) {
                    mImageReader.close();
                    mImageReader = null;
                }
            } catch (InterruptedException e) {
                throw new RuntimeException("Interrupted while trying to lock camera closing.", e);
            } finally {
                mOpenCloseLock.release();
            }

            if( mBackgroundThread != null ) {
                mBackgroundThread.quitSafely();
                try {
                    mBackgroundThread.join();
                    mBackgroundThread = null;
                    mBackgroundHandler = null;
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }

            Log.i("CloseCamera", "Closed");
            return 0;
        }

        public int GetCameraFocusMode()
        {
            int rv = 0;
            if (mAFMode == CaptureRequest.CONTROL_AF_MODE_AUTO) rv = 0;
            if (mAFMode == CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO || mAFMode == CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE) rv = 1;
            return rv;
        }

        public int SetCameraFocusMode( int mode_num ) {
            int new_mode = CaptureRequest.CONTROL_AF_MODE_AUTO;
            if( mode_num == 1 ) new_mode = CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO;
            if( isAFModeSupported(new_mode) == false ) return -1;
            mAFMode = new_mode;
            if( mCaptureSession == null ) return 0;
            mBackgroundHandler.post(new Runnable() {
                @Override
                public void run() {
                    SetRepeatingRequest();
                }
            });
            return 0;
        }
    }

    private int cameraAPI;
    private Object[] cam;

    public int OpenCamera( int cam_index, long user_data )
    {
        if( sdk < 24 ) { // < 7.0
            cameraAPI = 1;
            cam = new camera1[ 8 ];
        } else {
            cameraAPI = 2;
            cam = new camera2[ 8 ];
        }
        int i = 0;
        for( i = 0; i < cam.length; i++ ) if( cam[ i ] == null ) break;
        if( i >= cam.length )
        {
            Log.e("OpenCamera","No place for the new connection");
            return -1;
        }
        int rv;
        if( cameraAPI == 1 ) {
            cam[i] = new camera1();
            rv = ((camera1) cam[i]).open(cam_index, i, user_data);
            if (rv < 0) ((camera1) cam[i]).close();
        } else {
            cam[i] = new camera2();
            rv = ((camera2) cam[i]).open(cam_index, i, user_data);
            if (rv < 0) ((camera2) cam[i]).close();
        }
        if (rv < 0)
            cam[i] = null;
        else
            rv = i; //connection index
        return rv;
    }

    public int CloseCamera( int con_index )
    {
        int rv = -1;
        if( con_index >= 0 && con_index < cam.length ) {
            if( cam[ con_index ] != null ) {
                if( cameraAPI == 1 )
                    rv = ((camera1)cam[ con_index ]).close();
                else
                    rv = ((camera2)cam[ con_index ]).close();
                cam[ con_index ] = null;
            }
        }
        return rv;
    }

    public int GetCameraWidth( int con_index )
    {
        int rv = 0;
        if( con_index >= 0 && con_index < cam.length ) {
            if (cam[con_index] != null) {
                if( cameraAPI == 1 )
                    rv = ((camera1)cam[ con_index ]).mWidth;
                else
                    rv = ((camera2)cam[ con_index ]).mWidth;
            }
        }
        return rv;
    }

    public int GetCameraHeight( int con_index )
    {
        int rv = -0;
        if( con_index >= 0 && con_index < cam.length ) {
            if (cam[con_index] != null) {
                if( cameraAPI == 1 )
                    rv = ((camera1)cam[ con_index ]).mHeight;
                else
                    rv = ((camera2)cam[ con_index ]).mHeight;
            }
        }
        return rv;
    }

    public int GetCameraFormat( int con_index )
    {
        int rv = 0;
        if( con_index >= 0 && con_index < cam.length ) {
            if (cam[con_index] != null) {
                if( cameraAPI == 2 ) rv = 2;
            }
        }
        return rv;
    }

    public int GetCameraFocusMode( int con_index )
    {
        int rv = 0;
        if( con_index >= 0 && con_index < cam.length ) {
            if (cam[con_index] != null) {
                if( cameraAPI == 1 )
                    rv = ((camera1)cam[ con_index ]).GetCameraFocusMode();
                else
                    rv = ((camera2)cam[ con_index ]).GetCameraFocusMode();
            }
        }
        return rv;
    }

    public int SetCameraFocusMode( int con_index, int mode_num )
    {
        int rv = 0;
        if( con_index >= 0 && con_index < cam.length ) {
            if (cam[con_index] != null) {
                if( cameraAPI == 1 )
                    rv = ((camera1)cam[ con_index ]).SetCameraFocusMode( mode_num );
                else
                    rv = ((camera2)cam[ con_index ]).SetCameraFocusMode( mode_num );
            }
        }
        return rv;
    }

    //
    // File
    //

    private void dirChecker( String dir )
    {
        File f = new File( mLocation + dir );

        if( !f.isDirectory() )
        {
            f.mkdirs();
        }
    }

    private void UnZip( BufferedInputStream zipInputStream, String location )
    {
        mZipInputStream = zipInputStream;
        mLocation = location;

        dirChecker( "" );

        try {
            BufferedInputStream fin = mZipInputStream;
            ZipInputStream zin = new ZipInputStream( fin );
            ZipEntry ze = null;
            while( ( ze = zin.getNextEntry() ) != null )
            {
                Log.i( "Decompress", "Unzipping " + ze.getName() );

                if( ze.isDirectory() )
                {
                    dirChecker( ze.getName() );
                }
                else
                {
                    FileOutputStream fout = new FileOutputStream( mLocation + ze.getName() );
                    byte[] buffer = new byte[ 4096 ];
                    int length;
                    while( ( length = zin.read( buffer ) ) > 0 )
                    {
                        fout.write( buffer, 0, length );
                    }
                    zin.closeEntry();
                    fout.close();
                }
            }
            zin.close();
        } catch( Exception e ) {
            Log.e( "Decompress", "unzip", e );
        }
    }

    public int CheckAppResources( InputStream internalHashStream )
    {
        int changes = 0;
        int len2 = 0;
        int noExternalHash = 0;
        BufferedInputStream h1 = null;
        FileInputStream h2 = null;

        //Read internal hash:
        h1 = new BufferedInputStream( internalHashStream );
        try {
            internalHashLen = h1.read( internalHash );
            h1.close();
        } catch( Exception e ) {
            Log.e( "CopyResources", "Open hash (local)", e );
            return -1;
        }

        //Try to open external hash:
        String externalFilesDir = GetDir( "external_files" );
        if( externalFilesDir == null )
        {
            Log.e( "CopyResources", "Can't get external files dir" );
            return -2;
        }
        try {
            h2 = new FileInputStream( externalFilesDir + "/hash" );
        } catch( FileNotFoundException e ) {
            Log.i( "CopyResources", "Hash not found on external storage" );
            noExternalHash = 1;
            changes = 1;
        }

        if( noExternalHash == 0 )
        {
            //Compare internal and external hashes:
            try {
                byte[] buf2 = new byte[ 128 ];
                len2 = h2.read( buf2 );
                if( internalHashLen != len2 ) changes = 1;
                for( int i = 0; i < internalHashLen; i++ )
                {
                    if( internalHash[ i ] != buf2[ i ] ) changes = 1;
                }
                h2.close();
            } catch( Exception e ) {
                Log.e( "CopyResources", "Compare hashes", e );
            }
        }

        if( changes == 0 )
        {
            Log.i( "CopyResources", "All files in place" );
        }

        return changes;
    }

    public int UnpackAppResources( InputStream internalResourcesStream )
    {
        String externalFilesDir = GetDir( "external_files" );

        //Copy internal hash to external:
        try {
            FileOutputStream fout = new FileOutputStream( externalFilesDir + "/hash" );
            fout.write( internalHash, 0, internalHashLen );
            fout.close();
        } catch( Exception e ) {
            Log.e( "CopyResources", "Write hash (external)", e );
        }

        //Unpack all files:
        BufferedInputStream zipInputStream = new BufferedInputStream( internalResourcesStream );
        String unzipLocation = externalFilesDir + "/";
        UnZip( zipInputStream, unzipLocation );

        return 0;
    }
}
