package io.github.sdtransporter.pokewalker;

import android.content.Context;
import org.json.*;
import java.util.ArrayList;

final class SavedConsoles {
    static final class Entry {
        final String name,host;
        Entry(String name,String host){this.name=name;this.host=host;}
        String label(){return name+" · "+host;}
    }
    static ArrayList<Entry> load(Context context){
        ArrayList<Entry> result=new ArrayList<>();
        try {JSONArray list=new JSONArray(context.getSharedPreferences("consoles",0).getString("list","[]"));
            for(int n=0;n<list.length();n++){JSONObject item=list.getJSONObject(n);result.add(new Entry(item.getString("name"),item.getString("host")));}
        } catch(JSONException ignored){}
        return result;
    }
    private static boolean write(Context context,ArrayList<Entry> entries){
        JSONArray list=new JSONArray();
        try {for(Entry entry:entries){JSONObject item=new JSONObject();item.put("name",entry.name);item.put("host",entry.host);list.put(item);}}
        catch(JSONException ex){return false;}
        return context.getSharedPreferences("consoles",0).edit().putString("list",list.toString()).commit();
    }
    static boolean save(Context context,String name,String host){
        ArrayList<Entry> list=load(context);
        for(int n=0;n<list.size();n++)if(list.get(n).host.equalsIgnoreCase(host)){list.set(n,new Entry(name,host));return write(context,list);}
        list.add(new Entry(name,host));return write(context,list);
    }
    static boolean remove(Context context,int index){ArrayList<Entry> list=load(context);if(index<0||index>=list.size())return false;list.remove(index);return write(context,list);}
}
