// SPDX-License-Identifier: GPL-2.0-only
// Owner-drawn native controls: real keyboard/focus/disabled state, no image hit boxes.
void draw_control(const DRAWITEMSTRUCT& item) {
    if(classic_skin){classic_draw_control(item);return;}
    const auto found=g.controls.find(item.hwndItem);if(found==g.controls.end())return;
    const auto& c=found->second;
    const int w=item.rcItem.right,h=item.rcItem.bottom,id=GetDlgCtrlID(item.hwndItem);
    HDC dc=CreateCompatibleDC(item.hDC), source=CreateCompatibleDC(item.hDC);
    HBITMAP bitmap=CreateCompatibleBitmap(item.hDC,w*nitro::samples,h*nitro::samples);
    const auto old=SelectObject(dc,bitmap), oldsrc=SelectObject(source,g.base_layer);
    POINT at{0,0};MapWindowPoints(item.hwndItem,g.window,&at,1);
    StretchBlt(dc,0,0,w*nitro::samples,h*nitro::samples,source,at.x,at.y,w,h,SRCCOPY);
    SelectObject(source,oldsrc);DeleteDC(source);
    {
        Graphics gr(dc);gr.ScaleTransform(g.scale*nitro::samples,g.scale*nitro::samples);gr.SetSmoothingMode(SmoothingModeAntiAlias);
        gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        if(g.current_tab==TAB_SETTINGS && c.tab==TAB_SETTINGS)gr.Clear(Color(kArcadeCard));
        const float lw=float(w)/g.scale,lh=float(h)/g.scale;
        const bool disabled=(item.itemState&ODS_DISABLED)!=0,pressed=(item.itemState&ODS_SELECTED)!=0;
        const bool hot=g.hot==item.hwndItem&&!disabled;
        const float alpha=disabled?.42f:1.f;
        const RectF box(1,1+(pressed?1.f:0.f),lw-2,lh-3);
        switch(c.kind) {
        case Kind::TabBtn: {
            const bool selected=g.current_tab==c.pill;
            if(hot)fill_round(gr,box,8,SolidBrush(Color(0xff202643)));
            text(gr,c.pill==TAB_MODS?L"MOD":tr(c.label),0,23,FontStyleBold,box,selected?kArcadeGold:kTextLight,StringAlignmentCenter);
            break;
        }
        case Kind::Launch: {
            const auto saved=gr.Save();gr.TranslateTransform(3,pressed?4.f:0.f);
            GraphicsPath face;round_rect(face,rect(0,4,lw-27,lh-20),17);
            Matrix slant(1,0,-.17f,1,18,0);face.Transform(&slant);
            std::unique_ptr<GraphicsPath> shadow(face.Clone());Matrix offset;offset.Translate(0,8);shadow->Transform(&offset);
            Pen outline(Color(0xff050b23),10);SolidBrush ink{Color(kArcadeDark)};gr.DrawPath(&outline,shadow.get());gr.FillPath(&ink,shadow.get());
            LinearGradientBrush gold(PointF(0,0),PointF(0,lh),Color(hot?0xfffff08c:0xffffe857),Color(disabled?0xff7a7255:0xffffc62f));
            gr.DrawPath(&outline,&face);gr.FillPath(&gold,&face);
            const auto clipped=gr.Save();gr.SetClip(&face,CombineModeIntersect);
            for(int row=0;row<5;++row)for(int col=0;col<7;++col)if((row+col)%2==0){
                SolidBrush check(Color(BYTE(12+col*6),255,164,8));
                PointF p[]={{lw-175+col*25.f-row*4,4+row*23.f},{lw-150+col*25.f-row*4,4+row*23.f},{lw-154+col*25.f-row*4,27+row*23.f},{lw-179+col*25.f-row*4,27+row*23.f}};
                gr.FillPolygon(&check,p,4);
            }
            Pen shine(Color(0xfffff38a),4);gr.DrawLine(&shine,26.f,8.f,lw-30,8.f);gr.Restore(clipped);
            Pen edge(Color(0xffffe97c),1.5f);gr.DrawPath(&edge,&face);
            PointF triangle[]={{84,28},{78,69},{113,49}};gr.FillPolygon(&ink,triangle,3);
            const wchar_t* label=tr(g.process?L"IN GAME":(g.xbox_busy||g.shaders_busy)?L"WAIT":L"PLAY");
            text(gr,label,0,62,FontStyleBold,rect(128,9,lw-166,lh-25),kArcadeDark,StringAlignmentCenter);
            gr.Restore(saved);
            break;
        }
        case Kind::ActionBtn:case Kind::Browse: {
            const bool friends=id==ID_NITRO_FRIENDS,close=id==ID_CLOSE,mini=id==ID_MINIMIZE;
            if(id==ID_CHECK_UPDATE){
                if(hot)fill_round(gr,box,7,SolidBrush(Color(0xff252b49)));
                Pen refresh(Color(kTextLight),2);refresh.SetStartCap(LineCapRound);
                SolidBrush icon{Color(kTextLight)};
                for(float end:{-10.f,170.f}){
                    gr.DrawArc(&refresh,12.f,9.f,16.f,16.f,end-130,130.f);
                    const float a=end*3.14159265f/180.f;
                    const PointF tip(20+8*std::cos(a),17+8*std::sin(a));
                    const PointF direction(-std::sin(a),std::cos(a));
                    const PointF normal(std::cos(a),std::sin(a));
                    PointF arrow[]={{tip.X+direction.X*2,tip.Y+direction.Y*2},
                        {tip.X-direction.X*4+normal.X*3,tip.Y-direction.Y*4+normal.Y*3},
                        {tip.X-direction.X*4-normal.X*3,tip.Y-direction.Y*4-normal.Y*3}};
                    gr.FillPolygon(&icon,arrow,3);
                }
                text(gr,tr(c.label),2,13,FontStyleItalic,rect(36,0,lw-39,lh),kTextLight,StringAlignmentCenter);break;
            }
            const ARGB color=friends||close?kArcadePink:mini?kArcadeCyan:hot?0xff2c365c:0xff202742;
            fill_round(gr,box,8,SolidBrush(Color(fade(color,alpha))));
            if(!friends&&!close&&!mini)stroke_round(gr,box,8,0xff3c466c,1);
            const auto label=id==ID_NITRO_MOTION?tr(nitro_motion?L"Animations: on":L"Animations: off"):tr(c.label);
            if(friends){
                SolidBrush icon{Color(kArcadeDark)};gr.FillEllipse(&icon,20.f,8.f,9.f,9.f);gr.FillEllipse(&icon,29.f,10.f,7.f,7.f);
                gr.FillPie(&icon,15.f,18.f,18.f,17.f,180.f,180.f);gr.FillPie(&icon,27.f,19.f,13.f,14.f,180.f,180.f);
                text(gr,label,0,18,FontStyleBold,rect(45,1,lw-51,lh-3),kArcadeDark,StringAlignmentCenter);
            } else text(gr,label,2,mini||close?21.f:13.f,FontStyleBold,box,fade(close||mini?kArcadeDark:kTextLight,alpha),StringAlignmentCenter);
            break;
        }
        case Kind::Pill:case Kind::Segment: {
            const bool selected=c.kind==Kind::Pill?g.resolution==c.pill:g.mods_view==c.pill;
            fill_round(gr,box,7,SolidBrush(Color(fade(selected?kArcadeGold:hot?0xff29314f:kArcadeCardSub,alpha))));
            stroke_round(gr,box,7,selected?kArcadeGold:0xff3c466c,1);
            text(gr,tr(c.label),2,13,FontStyleBold,box,fade(selected?kArcadeDark:kTextLight,alpha),StringAlignmentCenter);
            break;
        }
        case Kind::Toggle: {
            const bool on=*c.value;const float y=(lh-22)/2;
            fill_round(gr,rect(1,y,40,22),11,SolidBrush(Color(fade(on?kArcadeCyan:0xff48526c,alpha))));
            SolidBrush knob{Color(fade(kTextLight,alpha))};gr.FillEllipse(&knob,on?22.f:4.f,y+3,16.f,16.f);
            text(gr,tr(c.label),2,13,FontStyleRegular,rect(51,0,lw-53,lh),fade(kTextLight,alpha));break;
        }
        }
        if((item.itemState&ODS_FOCUS) && !(item.itemState&ODS_NOFOCUSRECT))stroke_round(gr,rect(3,3,lw-6,lh-6),6,kArcadeCyan,1.5f);
    }
    SelectObject(dc,old);nitro_resolve(item.hDC,bitmap,w,h);DeleteObject(bitmap);DeleteDC(dc);
}
