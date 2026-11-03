package io.github.sdtransporter.pokewalker;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebResourceRequest;
import android.widget.Button;
import android.widget.LinearLayout;
import java.io.InputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;

/** Local policy/credits pages; no tracking, scripts or remote page loads. */
public final class PrivacyActivity extends Activity {
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout root=new LinearLayout(this);root.setOrientation(LinearLayout.VERTICAL);
        Button back=new Button(this);back.setText("Back");back.setOnClickListener(v->finish());root.addView(back);
        WebView page=new WebView(this);page.getSettings().setJavaScriptEnabled(false);
        page.getSettings().setAllowFileAccess(false);page.getSettings().setAllowContentAccess(false);
        page.setWebViewClient(new WebViewClient(){
            @Override public boolean shouldOverrideUrlLoading(WebView v,WebResourceRequest request){
                String scheme=request.getUrl().getScheme();
                if("walker-license".equals(scheme)){startActivity(new Intent(PrivacyActivity.this,PrivacyActivity.class).putExtra("licence",true));return true;}
                if("https".equals(scheme)||"mailto".equals(scheme)) {
                    try{startActivity(new Intent(Intent.ACTION_VIEW,request.getUrl()));}catch(Exception ignored){}
                }
                return true;
            }
        });
        root.addView(page,new LinearLayout.LayoutParams(-1,0,1));setContentView(root);
        String asset=getIntent().getBooleanExtra("licence",false)?"licence.html":getIntent().getBooleanExtra("credits",false)?"credits.html":"privacy.html";
        try(InputStream in=getAssets().open(asset);ByteArrayOutputStream out=new ByteArrayOutputStream()) {
            byte[] block=new byte[4096];int n;while((n=in.read(block))!=-1)out.write(block,0,n);
            page.loadDataWithBaseURL(null,new String(out.toByteArray(),StandardCharsets.UTF_8),"text/html","UTF-8",null);
        }catch(Exception ex){page.loadData("The local policy page could not be opened.","text/html","UTF-8");}
    }
}
