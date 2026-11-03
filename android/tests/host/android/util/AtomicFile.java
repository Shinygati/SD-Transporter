package android.util;
import java.io.*;import java.nio.file.*;
public class AtomicFile {
 public static boolean fail,silentDrop;private final File base,tmp;
 public AtomicFile(File p){base=p;tmp=new File(p+".new");}
 public File getBaseFile(){return base;}
 public InputStream openRead()throws IOException{return new FileInputStream(base);}
 public FileOutputStream startWrite()throws IOException{if(fail)throw new IOException("injected storage failure");return new FileOutputStream(tmp);}
 public void finishWrite(FileOutputStream s){try{s.close();if(silentDrop){tmp.delete();return;}Files.move(tmp.toPath(),base.toPath(),StandardCopyOption.REPLACE_EXISTING);}catch(IOException ex){throw new UncheckedIOException(ex);}}
 public void failWrite(FileOutputStream s){try{s.close();}catch(IOException ignored){}tmp.delete();}
}
