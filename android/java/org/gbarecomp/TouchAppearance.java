package org.gbarecomp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;

/** Vector controls shared by the layout preview; mirrors the native glyphs. */
final class TouchAppearance {
    interface Words { String text(String es, String en, String pt); }
    static final int CLASSIC = 0;
    static final int SYMBOLS = 1;
    static final int DEFAULT_OPACITY = 75;
    static final int MIN_OPACITY = 20;

    private TouchAppearance() {}

    static int style(Context context) {
        return context.getSharedPreferences("sma3-options", Context.MODE_PRIVATE)
                .getInt("touch-style", SYMBOLS) == CLASSIC ? CLASSIC : SYMBOLS;
    }

    static int opacity(Context context) {
        return Math.max(MIN_OPACITY, Math.min(100,
                context.getSharedPreferences("sma3-options", Context.MODE_PRIVATE)
                        .getInt("touch-opacity", DEFAULT_OPACITY)));
    }

    static String[] actionNames(Words words) {
        return new String[]{
            words.text("Cruceta · Mover", "D-pad · Move", "Direcional · Mover"),
            words.text("A · Saltar", "A · Jump", "A · Pular"),
            words.text("B · Lengua", "B · Tongue", "B · Língua"),
            words.text("L · Fijar mira", "L · Lock aim", "L · Fixar mira"),
            words.text("R · Huevo", "R · Egg", "R · Ovo"),
            words.text("Select · Estado", "Select · Status", "Select · Estado"),
            words.text("Start · Pausa", "Start · Pause", "Start · Pausa")
        };
    }

    static String[] descriptions(Words words) {
        return new String[]{
            words.text("Flechas: moverse. Abajo: agacharse o tragar; en el aire, pisotón.",
                "Arrows: move. Down: crouch or swallow; in mid-air, ground pound.",
                "Setas: mover. Baixo: agachar ou engolir; no ar, ataque ao chão."),
            words.text("Flecha hacia arriba: saltar. Mantener A permite aletear.",
                "Up arrow: jump. Hold A to flutter.",
                "Seta para cima: pular. Segure A para flutuar."),
            words.text("Lengua: comer o escupir. La acción depende de lo que Yoshi tenga en la boca.",
                "Tongue: eat or spit. The action depends on what Yoshi is holding in his mouth.",
                "Língua: comer ou cuspir. A ação depende do que Yoshi tem na boca."),
            words.text("Diana: fijar o liberar el cursor al apuntar un huevo.",
                "Target: lock or release the cursor while aiming an egg.",
                "Alvo: fixar ou liberar o cursor ao mirar um ovo."),
            words.text("Huevo: apuntar y lanzar según el estilo de lanzamiento del juego.",
                "Egg: aim and throw according to the game's throwing style.",
                "Ovo: mirar e lançar conforme o estilo de arremesso do jogo."),
            words.text("Lista: abrir la pantalla de estado.", "List: open the status screen.",
                "Lista: abrir a tela de estado."),
            words.text("Dos barras: pausar el juego.", "Two bars: pause the game.",
                "Duas barras: pausar o jogo.")
        };
    }

    static void drawControl(Canvas canvas, Paint paint, int id, RectF bounds,
                            int style, int opacity) {
        int alpha = (255 * Math.max(MIN_OPACITY, Math.min(100, opacity)) + 50) / 100;
        paint.setStyle(Paint.Style.FILL);
        paint.setColor((alpha << 24) | (id == 1 ? 0xb13141 : id == 2 ? 0x2469ab : 0x14231b));
        float edge = Math.max(1f, Math.min(bounds.width(), bounds.height()) * .029f);
        if (id == 0) {
            drawDpad(canvas, paint, bounds, alpha, edge);
            return;
        }
        if (id < 3) canvas.drawOval(bounds, paint);
        else canvas.drawRoundRect(bounds, bounds.height() * .25f, bounds.height() * .25f, paint);
        paint.setColor((alpha << 24) | 0xe2f2de);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(edge);
        if (id < 3) canvas.drawOval(bounds, paint);
        else canvas.drawRoundRect(bounds, bounds.height() * .25f, bounds.height() * .25f, paint);
        paint.setColor((alpha << 24) | 0xffffff);
        if (style == CLASSIC) {
            String[] labels = {"", "A", "B", "L", "R", "SELECT", "START"};
            drawLabel(canvas, paint, labels[id], bounds);
        } else drawSymbol(canvas, paint, id, bounds);
        paint.setStyle(Paint.Style.FILL);
    }

    private static void drawDpad(Canvas canvas, Paint paint, RectF r, int alpha, float edge) {
        float cx = r.centerX(), cy = r.centerY(), arm = r.width() * .5f;
        float half = r.width() * (9f / 23f) * .5f;
        // One closed cross, without doubled translucency in the centre.
        Path path = new Path();
        path.moveTo(cx-half,cy-arm); path.lineTo(cx+half,cy-arm);
        path.lineTo(cx+half,cy-half); path.lineTo(cx+arm,cy-half);
        path.lineTo(cx+arm,cy+half); path.lineTo(cx+half,cy+half);
        path.lineTo(cx+half,cy+arm); path.lineTo(cx-half,cy+arm);
        path.lineTo(cx-half,cy+half); path.lineTo(cx-arm,cy+half);
        path.lineTo(cx-arm,cy-half); path.lineTo(cx-half,cy-half); path.close();
        canvas.drawPath(path, paint);
        paint.setColor((alpha << 24) | 0xe2f2de);
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(edge);
        canvas.drawPath(path, paint);
        float arrow = Math.min(half * .44f, (arm - half) * .3f);
        float offset = half + (arm - half) * .58f;
        paint.setColor((alpha << 24) | 0xffffff);
        paint.setStrokeWidth(Math.max(.75f, arrow * .35f));
        for (int direction = 0; direction < 4; ++direction) {
            float dx = direction == 2 ? -1f : direction == 3 ? 1f : 0f;
            float dy = direction == 0 ? -1f : direction == 1 ? 1f : 0f;
            float x = cx + dx * offset, y = cy + dy * offset;
            canvas.drawLine(x-dx*arrow-dy*arrow,y-dy*arrow+dx*arrow,
                x+dx*arrow,y+dy*arrow,paint);
            canvas.drawLine(x+dx*arrow,y+dy*arrow,
                x-dx*arrow+dy*arrow,y-dy*arrow-dx*arrow,paint);
        }
        paint.setStyle(Paint.Style.FILL);
    }

    private static void line(Canvas canvas, Paint paint, RectF b,
                             float x0, float y0, float x1, float y1) {
        canvas.drawLine(b.left + x0 * b.width(), b.top + y0 * b.height(),
                b.left + x1 * b.width(), b.top + y1 * b.height(), paint);
    }

    private static void path(Canvas canvas, Paint paint, RectF b, float... points) {
        Path path = new Path();
        path.moveTo(b.left + points[0]*b.width(), b.top + points[1]*b.height());
        for (int i=2;i<points.length;i+=2)
            path.lineTo(b.left+points[i]*b.width(),b.top+points[i+1]*b.height());
        canvas.drawPath(path,paint);
    }

    private static void dot(Canvas canvas, Paint paint, RectF b, float x, float y, float radius) {
        paint.setStyle(Paint.Style.FILL);
        canvas.drawCircle(b.left+x*b.width(),b.top+y*b.height(),radius*b.width(),paint);
        paint.setStyle(Paint.Style.STROKE);
    }

    private static void drawSymbol(Canvas canvas, Paint paint, int id, RectF bounds) {
        float side = Math.min(bounds.width(), bounds.height()) * .62f;
        RectF b = new RectF(bounds.centerX()-side*.5f,bounds.centerY()-side*.5f,
                bounds.centerX()+side*.5f,bounds.centerY()+side*.5f);
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(Math.max(.75f,side*.105f));
        switch (id) {
        case 1:
            line(canvas,paint,b,.5f,.12f,.5f,.76f);line(canvas,paint,b,.22f,.4f,.5f,.12f);
            line(canvas,paint,b,.5f,.12f,.78f,.4f);line(canvas,paint,b,.16f,.92f,.84f,.92f);break;
        case 2:
            path(canvas,paint,b,.12f,.22f,.12f,.68f,.26f,.80f,.76f,.80f,.90f,.66f,
                    .90f,.52f,.78f,.40f,.26f,.40f,.26f,.22f);
            line(canvas,paint,b,.53f,.43f,.53f,.63f);break;
        case 3:
            canvas.drawCircle(b.centerX(),b.centerY(),side*.28f,paint);
            line(canvas,paint,b,.5f,.02f,.5f,.24f);line(canvas,paint,b,.5f,.76f,.5f,.98f);
            line(canvas,paint,b,.02f,.5f,.24f,.5f);line(canvas,paint,b,.76f,.5f,.98f,.5f);
            dot(canvas,paint,b,.5f,.5f,.055f);break;
        case 4:
            paint.setStrokeWidth(paint.getStrokeWidth()*.8f);
            path(canvas,paint,b,.5f,.06f,.34f,.10f,.22f,.24f,.14f,.46f,.14f,.70f,
                    .25f,.86f,.5f,.94f,.75f,.86f,.86f,.70f,.86f,.46f,.78f,.24f,
                    .66f,.10f,.5f,.06f);
            dot(canvas,paint,b,.38f,.40f,.075f);dot(canvas,paint,b,.65f,.63f,.09f);
            dot(canvas,paint,b,.4f,.77f,.055f);break;
        case 5:
            for(float y:new float[]{.22f,.5f,.78f}){
                dot(canvas,paint,b,.16f,y,.055f);line(canvas,paint,b,.35f,y,.9f,y);
            }break;
        case 6:
            paint.setStyle(Paint.Style.FILL);
            canvas.drawRect(b.left+.2f*side,b.top+.1f*side,b.left+.4f*side,b.top+.9f*side,paint);
            canvas.drawRect(b.left+.6f*side,b.top+.1f*side,b.left+.8f*side,b.top+.9f*side,paint);
            break;
        default:break;
        }
    }

    private static void drawLabel(Canvas canvas, Paint paint, String text, RectF bounds) {
        int count = text.length();
        float height = Math.min(bounds.height()*.52f,bounds.width()*.78f/(count*.78f-.18f));
        float width=height*.60f, advance=height*.78f;
        float x=bounds.left+(bounds.width()-(advance*count-height*.18f))*.5f;
        float y=bounds.centerY()-height*.5f;
        paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(Math.max(.65f,height*.105f));
        for(int i=0;i<count;++i)
            letter(canvas,paint,text.charAt(i),new RectF(x+advance*i,y,x+advance*i+width,y+height));
    }

    private static void letter(Canvas c, Paint p, char ch, RectF b) {
        switch(ch){
        case 'A':line(c,p,b,0,1,.5f,0);line(c,p,b,.5f,0,1,1);line(c,p,b,.23f,.58f,.77f,.58f);break;
        case 'B':
            line(c,p,b,0,0,0,1);line(c,p,b,0,0,.72f,0);line(c,p,b,.72f,0,1,.2f);
            line(c,p,b,1,.2f,.72f,.5f);line(c,p,b,.72f,.5f,0,.5f);
            line(c,p,b,.72f,.5f,1,.75f);line(c,p,b,1,.75f,.72f,1);line(c,p,b,.72f,1,0,1);break;
        case 'C':
            line(c,p,b,1,.1f,.25f,.1f);line(c,p,b,.25f,.1f,0,.3f);
            line(c,p,b,0,.3f,0,.7f);line(c,p,b,0,.7f,.25f,.9f);line(c,p,b,.25f,.9f,1,.9f);break;
        case 'E':
            line(c,p,b,0,0,0,1);line(c,p,b,0,0,1,0);line(c,p,b,0,.5f,.8f,.5f);line(c,p,b,0,1,1,1);break;
        case 'L':line(c,p,b,0,0,0,1);line(c,p,b,0,1,1,1);break;
        case 'R':
            line(c,p,b,0,0,0,1);line(c,p,b,0,0,.72f,0);line(c,p,b,.72f,0,1,.2f);
            line(c,p,b,1,.2f,.72f,.5f);line(c,p,b,.72f,.5f,0,.5f);line(c,p,b,.5f,.5f,1,1);break;
        case 'S':
            line(c,p,b,1,.1f,.2f,.1f);line(c,p,b,.2f,.1f,0,.28f);line(c,p,b,0,.28f,.2f,.5f);
            line(c,p,b,.2f,.5f,.8f,.5f);line(c,p,b,.8f,.5f,1,.72f);
            line(c,p,b,1,.72f,.8f,.9f);line(c,p,b,.8f,.9f,0,.9f);break;
        case 'T':line(c,p,b,0,0,1,0);line(c,p,b,.5f,0,.5f,1);break;
        default:break;
        }
    }
}
