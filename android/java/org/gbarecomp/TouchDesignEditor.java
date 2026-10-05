package org.gbarecomp;
import android.app.Dialog;
import android.content.Context;
import android.graphics.*;
import android.os.Bundle;
import android.view.*;
import android.widget.*;
import java.util.function.Consumer;

/** A local draft: Cancel never modifies the active layout. */
final class TouchDesignEditor extends Dialog {
 interface Words {String text(String es,String en,String pt);}
 private final Words words; private final Consumer<float[]> save;
 private final float[] defaults,values; private final float aspect;
 private final String[] names; private int selected;
 private Preview preview;private SeekBar size;private Spinner selector;
 private float baseW,baseH;
 TouchDesignEditor(Context context,Words words,float[] initial,float[] defaults,float aspect,Consumer<float[]> save){
  super(context,android.R.style.Theme_Material_NoActionBar_Fullscreen);
  this.words=words;this.save=save;this.values=initial.clone();this.defaults=defaults.clone();this.aspect=aspect;
  names=new String[]{t("Cruceta","D-pad","Direcional"),"A","B","L","R","Select","Start"};
 }
 private String t(String es,String en,String pt){return words.text(es,en,pt);}
 private int dp(int n){return Math.round(n*getContext().getResources().getDisplayMetrics().density);}
 @Override protected void onCreate(Bundle state){
  super.onCreate(state);
  LinearLayout root=new LinearLayout(getContext());root.setOrientation(LinearLayout.VERTICAL);root.setPadding(dp(12),dp(8),dp(12),dp(8));root.setBackgroundColor(0xff101724);
  TextView help=new TextView(getContext());help.setText(t("Arrastra los controles · Juego en pausa","Drag the controls · Game paused","Arraste os controles · Jogo pausado"));help.setTextColor(Color.WHITE);help.setTextSize(15);root.addView(help);
  preview=new Preview(getContext());root.addView(preview,new LinearLayout.LayoutParams(-1,0,1));
  LinearLayout row=new LinearLayout(getContext());selector=new Spinner(getContext());selector.setAdapter(new ArrayAdapter<String>(getContext(),android.R.layout.simple_spinner_dropdown_item,names));row.addView(selector,new LinearLayout.LayoutParams(dp(125),dp(48)));
  size=new SeekBar(getContext());size.setMax(150);size.setContentDescription(t("Tamaño del control seleccionado","Selected control size","Tamanho do controle selecionado"));row.addView(size,new LinearLayout.LayoutParams(0,dp(48),1));TextView label=new TextView(getContext());label.setText(t("Tamaño","Size","Tamanho"));label.setTextColor(Color.WHITE);row.addView(label);root.addView(row);
  selector.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onNothingSelected(AdapterView<?> p){}public void onItemSelected(AdapterView<?> p,View v,int pos,long id){select(pos);}});
  size.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){
   public void onStartTrackingTouch(SeekBar b){}public void onStopTrackingTouch(SeekBar b){}
   public void onProgressChanged(SeekBar b,int n,boolean user){if(!user)return;float scale=Math.min((n+50)/100f,Math.min(1/baseW,1/baseH));values[selected*4+2]=Math.max(.015f,baseW*scale);values[selected*4+3]=Math.max(.015f,baseH*scale);clamp(selected);preview.invalidate();}
  });
  LinearLayout actions=new LinearLayout(getContext());button(actions,t("Cancelar","Cancel","Cancelar"),this::cancel);button(actions,t("Restablecer","Reset","Restaurar"),()->{System.arraycopy(defaults,0,values,0,28);select(selected);preview.invalidate();});button(actions,t("Guardar y usar","Save and use","Salvar e usar"),()->{save.accept(values.clone());dismiss();});root.addView(actions);
  setContentView(root);select(0);getWindow().setLayout(-1,-1);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
 }
 private void button(LinearLayout row,String text,Runnable run){Button b=new Button(getContext());b.setText(text);b.setTextSize(12);b.setOnClickListener(v->run.run());row.addView(b,new LinearLayout.LayoutParams(0,dp(48),1));}
 private void select(int id){selected=id;baseW=values[id*4+2];baseH=values[id*4+3];size.setProgress(50);preview.invalidate();}
 private void clamp(int id){int i=id*4;values[i]=Math.max(values[i+2]/2,Math.min(1-values[i+2]/2,values[i]));values[i+1]=Math.max(values[i+3]/2,Math.min(1-values[i+3]/2,values[i+1]));}
 private final class Preview extends View {
  private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);private final RectF screen=new RectF();private int pointer=-1;private float dx,dy;
  Preview(Context c){super(c);setContentDescription(t("Vista previa del diseño de controles","Control layout preview","Prévia do layout dos controles"));}
  private RectF rect(int id){int i=id*4;float w=values[i+2]*screen.width(),h=values[i+3]*screen.height();if(id<3)w=h=Math.min(w,h);float cx=screen.left+values[i]*screen.width(),cy=screen.top+values[i+1]*screen.height();return new RectF(cx-w/2,cy-h/2,cx+w/2,cy+h/2);}
  @Override protected void onDraw(Canvas c){
   super.onDraw(c);float w=Math.max(1,getWidth()-dp(8)),h=Math.max(1,getHeight()-dp(8));if(w/h>aspect)w=h*aspect;else h=w/aspect;screen.set((getWidth()-w)/2,(getHeight()-h)/2,(getWidth()+w)/2,(getHeight()+h)/2);
   paint.setStyle(Paint.Style.FILL);paint.setColor(0xff252f40);c.drawRoundRect(screen,dp(8),dp(8),paint);
   for(int id=0;id<7;id++){RectF r=rect(id);paint.setColor(id==selected?0xff278cbb:0xff59677c);
    if(id==0){float t=r.width()*.39f;c.drawRect(r.centerX()-t/2,r.top,r.centerX()+t/2,r.bottom,paint);c.drawRect(r.left,r.centerY()-t/2,r.right,r.centerY()+t/2,paint);}else if(id<3)c.drawOval(r,paint);else c.drawRoundRect(r,dp(6),dp(6),paint);
    if(id==selected){paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(dp(2));paint.setColor(Color.CYAN);c.drawRect(r,paint);paint.setStyle(Paint.Style.FILL);}
    paint.setColor(Color.WHITE);paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(Math.min(dp(14),r.height()*.5f));c.drawText(id==0?"+":names[id],r.centerX(),r.centerY()-(paint.ascent()+paint.descent())/2,paint);
   }
  }
  @Override public boolean onTouchEvent(MotionEvent e){
   if(screen.width()<=0||screen.height()<=0)return true;
   switch(e.getActionMasked()){
    case MotionEvent.ACTION_DOWN:
     int hit=-1;if(rect(selected).contains(e.getX(),e.getY()))hit=selected;else for(int i=6;i>=0;i--)if(rect(i).contains(e.getX(),e.getY())){hit=i;break;}if(hit<0)return true;
     if(hit!=selected){selector.setSelection(hit);select(hit);}pointer=e.getPointerId(0);dx=values[selected*4]-(e.getX()-screen.left)/screen.width();dy=values[selected*4+1]-(e.getY()-screen.top)/screen.height();return true;
    case MotionEvent.ACTION_MOVE:
     int index=e.findPointerIndex(pointer);if(index<0)return true;values[selected*4]=(e.getX(index)-screen.left)/screen.width()+dx;values[selected*4+1]=(e.getY(index)-screen.top)/screen.height()+dy;clamp(selected);invalidate();return true;
    case MotionEvent.ACTION_POINTER_UP:if(e.getPointerId(e.getActionIndex())==pointer)pointer=-1;return true;
    case MotionEvent.ACTION_UP:performClick();pointer=-1;return true;
    case MotionEvent.ACTION_CANCEL:pointer=-1;return true;
    default:return true;
   }
  }
  @Override public boolean performClick(){super.performClick();return true;}
 }
}
