(*
This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

Copyright (c) Alexey Torgashin
*)
{$ifdef nn}begin end;{$endif}

procedure TfmMain.FinderOnFound(Sender: TObject; APos1, APos2: TPoint);
var
  Ed: TATSynEdit;
  NLen: integer;
begin
  Ed:= FFinder.Editor;
  case FFindMarkingMode of
    markingNone:
      begin
        Ed.DoGotoPos(
          APos1,
          APos2,
          UiOps.FindIndentHorz,
          UiOps.FindIndentVert,
          true,
          true
          );
      end;

    markingSelections:
      begin
        if FFindMarkingCaret1st then
        begin
          FFindMarkingCaret1st:= false;
          Ed.Carets.Clear;
        end;
        Ed.Carets.Add(APos2.X, APos2.Y, APos1.X, APos1.Y);
        //Ed.Carets.Sort; //better comment, to not merge many selections, on "Select all"
      end;

    markingMarkers:
      begin
        //marker sel is only 1 line
        if APos2.Y=APos1.Y then
          NLen:= Abs(APos2.X-APos1.X)
        else
          NLen:= 0;

        //ShowMessage(Format('mark at %d:%d', [APos1.Y, APos1.X]));//debug
        Ed.Markers.Add(APos1.X, APos1.Y, 0, NLen, 0, nil, true{InsertToBegin});
      end;

    markingBookmarks:
      begin
        ed.BookmarkSetForLine(APos1.Y, 1, '', false);
      end;
  end;
end;


procedure TfmMain.DoFindOptions_ResetInSelection;
begin
  //todo: maybe rework and dont reset opt
  FFinder.OptInSelection:= false;
  if Assigned(fmFind) then
    fmFind.chkInSel.Checked:= false;
end;

procedure TfmMain.FindDialogDone(Sender: TObject; const Res: string);
var
  ok, bChanged, bDoLock, bFindMode: boolean;
  cnt, NTop, NLeft,
  NCaretX, NCaretY: integer;
  NTick, NTime: QWord;
  Frame: TEditorFrame;
begin
  InitFormFind;
  Frame:= CurrentFrame;

  //keep focus in Find field, else focus find or rep field
  if fmFind.edFind.Focused then
    bFindMode:= true
  else
  if fmFind.edRep.Focused then
    bFindMode:= false
  else
    bFindMode:=
      (Res<>cOpFindRep) and
      (Res<>cOpFindRepAll) and
      (Res<>cOpFindRepAndStop);

  if Res=cOpFindClose then
  begin
    //handy to reset in-sel, on closing dialog
    FFinder.OptInSelection:= false;

    if IsFocusedFind then
      Frame.SetFocus;
    fmFind.Hide;
    UpdateAppForSearch(false, false, bFindMode);
    Exit;
  end;

  //Sender=nil allows to not use dialog
  if Assigned(Sender) then
  with fmFind do
  begin
    if edFind.Text='' then Exit;
    FFinder.StrFind:= edFind.Text;
    FFinder.StrReplace:= edRep.Text;
    FFinder.OptBack:= false;
    FFinder.OptCase:= chkCase.Checked;
    FFinder.OptWords:= chkWords.Checked;
    FFinder.OptRegex:= chkRegex.Checked;
    FFinder.OptWrapped:= chkWrap.Checked;
    FFinder.OptInSelection:= chkInSel.Checked;
    FFinder.OptFromCaret:= (Res=cOpFindNext) or (Res=cOpFindPrev) or (Res=cOpFindRep);
    FFinder.OptConfirmReplace:= chkConfirm.Checked;
  end;

  if Frame.IsBinary then
  begin
    UpdateAppForSearch(true, false, true);
    Application.ProcessMessages;

    case Res of
      cOpFindFirst:
        begin
          if Frame.BinaryFindFirst(FFinder, false) then
            MsgStatus(msgStatusFoundNextMatch)
          else
            MsgStatus(msgCannotFindMatch);
        end;

      cOpFindNext,
      cOpFindPrev:
        begin
          if Frame.BinaryFindNext(Res=cOpFindPrev) then
            MsgStatus(msgStatusFoundNextMatch)
          else
            MsgStatus(msgCannotFindMatch);
        end;
    end;

    UpdateAppForSearch(false, false, true);
    exit;
  end;

  //if FFinder.OptInSelection then
  //  if CurrentEditor.Carets.Count>1 then
  //    MsgBox(msgCannotFindInMultiSel, MB_OK+MB_ICONWARNING);
  if Frame.MacroRecord then
    Frame.MacroString:= Frame.MacroString+(Format('%d,%s'#1'%s'#1'%s'#1'%s',
      [cmd_FinderAction, Res, FFinder.StrFind, FFinder.StrReplace, DoFindOptionsToString])+#10);

  bDoLock:=
    (Res=cOpFindRep) or
    (Res=cOpFindRepAndStop) or
    (Res=cOpFindRepAll);
  UpdateAppForSearch(true, bDoLock, bFindMode);

  if (Res=cOpFindFirst) or (Res=cOpFindNext) or (Res=cOpFindPrev) then
  begin
    FFinder.OptBack:= (Res=cOpFindPrev);
    ok:= FFinder.DoAction_FindOrReplace(false, false, false, bChanged);
    FinderUpdateEditor(false);
    DoFindResult(ok);
    if ok then
      DoFindOptions_ResetInSelection;
  end;

  if (Res=cOpFindRep) or (Res=cOpFindRepAndStop) then
  begin
    //replace match
    ok:= FFinder.DoAction_ReplaceSelected;
    //after match is replaced, do find-next
    if Res=cOpFindRep then
      if ok then
        ok:= FFinder.DoAction_FindOrReplace(false, false, false, bChanged);
    FinderUpdateEditor(true);
    DoFindResult(ok);
    if ok then
      DoFindOptions_ResetInSelection;
  end;

  if Res=cOpFindRepAll then
  begin
    NTop:= FFinder.Editor.LineTop;
    NLeft:= FFinder.Editor.ColumnLeft;
    with FFinder.Editor do
      if Carets.Count>0 then
      begin
        NCaretX:= Carets[0].PosX;
        NCaretY:= Carets[0].PosY;
      end;

    NTick:= GetTickCount64;
    FFinder.Editor.Strings.BeginUndoGroup;
    cnt:= FFinder.DoAction_ReplaceAll;
    FFinder.Editor.Strings.EndUndoGroup;
    FinderUpdateEditor(true);
    NTime:= (GetTickCount64-NTick) div 1000;

    FFinder.Editor.DoCaretSingle(NCaretX, NCaretY);
    FFinder.Editor.LineTop:= NTop;
    FFinder.Editor.ColumnLeft:= NLeft;
    MsgStatus(
      Format(msgStatusReplaceCount, [cnt])+
      Format(' (%d s)', [NTime]) );
  end;

  if Res=cOpFindCount then
  begin
    DoFindMarkingInit(markingNone);
    cnt:= FFinder.DoAction_CountAll(false);
    MsgStatus(Format(msgStatusFindCount, [Utf8Encode(FFinder.StrFind), cnt]));
  end;

  if Res=cOpFindSelectAll then
  begin
    DoFindMarkingInit(markingSelections);
    cnt:= FFinder.DoAction_CountAll(true);
    DoFindMarkingInit(markingNone);
    FinderUpdateEditor(false);
    MsgStatus(
      Format(msgStatusFindCount, [Utf8Encode(FFinder.StrFind), cnt]) +
      ' ('+SFindOptionsToTextHint+')');
    DoFindOptions_ResetInSelection;
  end;

  if Res=cOpFindMarkAll then
  begin
    DoFindMarkingInit(markingMarkers);
    cnt:= FFinder.DoAction_CountAll(true);
    DoFindMarkingInit(markingNone);
    FinderUpdateEditor(false);
    MsgStatus(
      Format(msgStatusFindCount, [Utf8Encode(FFinder.StrFind), cnt]) +
      ' ('+SFindOptionsToTextHint+')');
  end;

  UpdateAppForSearch(false, false, bFindMode);
end;

procedure TfmMain.InitFormFind;
var
  cfg: TJsonConfig;
begin
  if not Assigned(fmFind) then
  begin
    fmFind:= TfmFind.Create(Self);
    fmFind.OnResult:= @FindDialogDone;
    fmFind.OnChangeOptions:= @DoFindOptions_OnChange;
    fmFind.Color:= GetAppColor('TabBg');

    UiOps.HotkeyFindDialog:= ShortcutToText(AppKeymap.GetShortcutFromCommand(cmd_DialogFind));
    UiOps.HotkeyReplaceDialog:= ShortcutToText(AppKeymap.GetShortcutFromCommand(cmd_DialogReplace));

    if not UiOps.FindSeparateForm then
    begin
      fmFind.Parent:= PanelMain;
      fmFind.Align:= alBottom;
      fmFind.BorderStyle:= bsNone;
    end
    else
    begin
      fmFind.IsNarrow:= true;
      fmFind.Width:= 700;
      fmFind.Constraints.MinWidth:= 400;
      if UiOps.ShowFormsOnTop then
        fmFind.FormStyle:= fsSystemStayOnTop;
      //else: it's already fsStayOnTop
    end;

    cfg:= TJSONConfig.Create(nil);
    try
      cfg.Filename:= GetAppPath(cFileOptionsHistory);
      SLoadStringsFromFile(cfg, '/list_find', fmFind.edFind.Items, UiOps.MaxHistoryEdits);
      SLoadStringsFromFile(cfg, '/list_replace', fmFind.edRep.Items, UiOps.MaxHistoryEdits);
      if fmFind.IsNarrow then
      begin
        fmFind.Left:= cfg.GetValue('/find/pos_x', 200);
        fmFind.Top:= cfg.GetValue('/find/pos_y', 400);
        fmFind.Width:= cfg.GetValue('/find/size_x', fmFind.Width);
      end;
      fmFind.IsMultiLine:= cfg.GetValue('/find/multi_line', false);
    finally
      cfg.Free
    end;
  end;

  DoLocalize_FormFind;
end;

procedure TfmMain.DoDialogFind(AReplaceMode: boolean);
var
  Ed: TATSynEdit;
  StrSel, StrWord: atString;
  bMultiLineSel: boolean;
  Caret: TATCaretItem;
begin
  Ed:= CurrentEditor;

  StrWord:= Ed.TextCurrentWord;
  StrSel:= Ed.TextSelected;

  //ignore multi-line sel
  if Pos(#10, StrSel)>0 then StrSel:= '';
  if Pos(#13, StrSel)>0 then StrSel:= '';
  if not Ed.IsSelRectEmpty then StrSel:= '';

  bMultiLineSel:= false;
  if Ed.Carets.Count=1 then
  begin
    Caret:= Ed.Carets[0];
    bMultiLineSel:= (Caret.EndY>=0) and (Caret.EndY<>Caret.PosY);
  end;

  MsgLogDebug('find dlg: init');
  InitFormFind;
  with fmFind do
  begin
    bFindFirst.Visible:= UiOps.FindShow_FindFirst;
    bSelectAll.Visible:= UiOps.FindShow_SelectAll;
    bMarkAll.Visible:= UiOps.FindShow_MarkALl;

    //change Find field only if options SuggestSel/SuggestWord on,
    //else dont touch Find field
    if UiOps.FindSuggestSel and (StrSel<>'') then
      edFind.Text:= StrSel
    else
    if UiOps.FindSuggestWord and (StrWord<>'') then
      edFind.Text:= StrWord;

    MsgLogDebug('find dlg: select all in input');
    edFind.DoCommand(cCommand_SelectAll);
    edRep.DoCommand(cCommand_SelectAll);

    chkCase.Checked:= FFinder.OptCase;
    chkWords.Checked:= FFinder.OptWords;
    chkRegex.Checked:= FFinder.OptRegex;
    chkConfirm.Checked:= FFinder.OptConfirmReplace;
    chkWrap.Checked:= FFinder.OptWrapped;

    if UiOps.FindSuggestInSelection and bMultiLineSel then
      chkInSel.Checked:= true
    else
      chkInSel.Checked:= FFinder.OptInSelection;

    IsReplace:= AReplaceMode;

    MsgLogDebug('find dlg: update state');
    UpdateState;
    MsgLogDebug('find dlg: show');
    Show;
    MsgLogDebug('find dlg: focus');
    UpdateFocus(true);
  end;
end;

procedure TfmMain.DoDialogFind_Hide;
begin
  if Assigned(fmFind) and fmFind.Visible then
  begin
    //handy to reset in-sel, on closing dialog
    FFinder.OptInSelection:= false;

    CurrentFrame.SetFocus;
    fmFind.Hide;
    UpdateAppForSearch(false, false, true);
  end;
end;


procedure TfmMain.DoFindFirst;
var
  ok, bChanged: boolean;
begin
  //if Assigned(fmFind) then
  //  FFinder.StrFind:= fmFind.edFind.Text;

  if FFinder.StrFind='' then
  begin
    DoDialogFind(false);
    Exit
  end;

  FFinder.Editor:= CurrentEditor;
  FFinder.OptBack:= false;
  FFinder.OptFromCaret:= false;

  ok:= FFinder.DoAction_FindOrReplace(false, false, false, bChanged);
  FinderUpdateEditor(false);
  DoFindResult(ok);
end;

procedure TfmMain.DoFindNext(ANext: boolean);
var
  ok, bChanged: boolean;
begin
  //if Assigned(fmFind) then
  //  FFinder.StrFind:= fmFind.edFind.Text;
  InitFormFind;

  if FFinder.StrFind='' then
  begin
    if fmFind.edFind.Text<>'' then
      FFinder.StrFind:= fmFind.edFind.Text
    else
    begin
      DoDialogFind(false);
      Exit
    end;
  end;

  FFinder.Editor:= CurrentEditor;
  FFinder.OptFromCaret:= true;
  if not ANext then
    FFinder.OptRegex:= false;
  FFinder.OptBack:= not ANext;

  ok:= FFinder.DoAction_FindOrReplace(false, false, false, bChanged);
  FinderUpdateEditor(false);
  DoFindResult(ok);
end;

procedure TfmMain.DoFindMarkingInit(AMode: TATFindMarkingMode);
var
  Ed: TATSynEdit;
begin
  Ed:= FFinder.Editor;
  if Ed=nil then
    raise Exception.Create('DoFindMarkingInit: Finder.Editor=nil');

  FFindMarkingMode:= AMode;
  FFindMarkingCaret1st:= true;

  case AMode of
    markingSelections:
      begin
      end;
    markingMarkers:
      begin
        Ed.Markers.Clear;
      end;
    markingBookmarks:
      begin
        Ed.DoCommand(cmd_BookmarkClearAll);
      end;
  end;
end;

procedure TfmMain.DoFindMarkAll(AMode: TATFindMarkingMode);
var
  cnt: integer;
begin
  //if Assigned(fmFind) then
  //  FFinder.StrFind:= fmFind.edFind.Text;

  if FFinder.StrFind='' then
  begin
    DoDialogFind(false);
    Exit
  end;

  FFinder.Editor:= CurrentEditor;
  FFinder.OptBack:= false;
  FFinder.OptFromCaret:= false;

  DoFindMarkingInit(AMode);
  cnt:= FFinder.DoAction_CountAll(true);
  DoFindMarkingInit(markingNone);

  FinderUpdateEditor(false);
  MsgStatus(Format(msgStatusFindCount, [Utf8Encode(FFinder.StrFind), cnt]));
end;


procedure TfmMain.FinderOnProgress(Sender: TObject; const ACurPos, AMaxPos: Int64;
  var AContinue: boolean);
var
  NValue: Int64;
begin
  if AMaxPos<=0 then exit;
  NValue:= ACurPos * 100 div AMaxPos;
  StatusProgress.Progress:= NValue;
  Application.ProcessMessages;
  if FFindStop or Application.Terminated then
    AContinue:= false;
end;

function TfmMain.SFindOptionsToTextHint: string;
const
  Sep=', ';
begin
  Result:= '';
  if FFinder.OptRegex then Result:= Result+msgFinderHintRegex+Sep;
  if FFinder.OptCase then Result:= Result+msgFinderHintCase+Sep;
  if FFinder.OptWords then Result:= Result+msgFinderHintWords+Sep;
  if FFinder.OptBack then Result:= Result+msgFinderHintBack+Sep;
  if FFinder.OptWrapped then Result:= Result+msgFinderHintWrapped+Sep;
  if FFinder.OptInSelection then Result:= Result+msgFinderHintInSel+Sep;
  if FFinder.OptFromCaret then Result:= Result+msgFinderHintFromCaret+Sep;
  if SEndsWith(Result, Sep) then
    SetLength(Result, Length(Result)-Length(Sep));
end;

procedure TfmMain.DoFindResult(ok: boolean);
var
  SOptions: string;
begin
  SOptions:= SFindOptionsToTextHint;

  if ok then
  begin
    MsgStatus(msgStatusFoundNextMatch+' ('+SOptions+')');

    //these 3 lines- to scroll to found pos
    //(was bad, if found pos in folded blk)
    FFinder.Editor.EndUpdate;
    Application.ProcessMessages;
    FFinder.Editor.DoGotoCaret(cEdgeTop);
  end
  else
    MsgStatus(msgCannotFindMatch+' ('+SOptions+'): '+Utf8Encode(FFinder.StrFind));
end;

procedure TfmMain.FinderOnConfirmReplace(Sender: TObject; APos1, APos2: TPoint;
  AForMany: boolean; var AConfirm, AContinue: boolean);
var
  Res: TModalResult;
  Ed: TATSynEdit;
  Pnt: TPoint;
begin
  case FFindConfirmAll of
    mrYesToAll: begin AConfirm:= true; exit end;
    mrNoToAll: begin AConfirm:= false; exit end;
  end;

  Ed:= (Sender as TATEditorFinder).Editor;

  with Ed.Carets[0] do
  begin
    PosX:= APos1.X;
    PosY:= APos1.Y;
    EndX:= APos2.X;
    EndY:= APos2.Y;
  end;

  Ed.EndUpdate;
  Ed.DoGotoCaret(cEdgeTop);
  Ed.Update(true);

  fmConfirmReplace:= TfmConfirmReplace.Create(Self);
  try
    DoLocalize_FormConfirmReplace(fmConfirmReplace);
    fmConfirmReplace.MsgLineNumber:= APos1.Y+1;
    fmConfirmReplace.bYesAll.Enabled:= AForMany;
    fmConfirmReplace.bNoAll.Enabled:= AForMany;
    if Assigned(fmFind) then
    begin
      Pnt:= fmFind.ClientToScreen(Point(0, 0));
      fmConfirmReplace.Left:= Pnt.X;
      fmConfirmReplace.Top:= Pnt.Y;
      //fmConfirmReplace.Width:= fmFind.Width;
    end;
    Res:= fmConfirmReplace.ShowModal;
  finally
    FreeAndNil(fmConfirmReplace);
  end;

  Ed.BeginUpdate;
  AConfirm:= Res in [mrYes, mrYesToAll];
  AContinue:= Res<>mrNoToAll;
  if Res in [mrYesToAll, mrNoToAll] then
    FFindConfirmAll:= Res;
end;

procedure TfmMain.FinderOnBadRegex(Sender: TObject);
begin
  MsgBox(
    msgStatusBadRegex+#10+Utf8Encode(FFinder.StrFind),
    MB_OK or MB_ICONERROR);
end;

procedure TfmMain.FinderUpdateEditor(AUpdateText: boolean);
var
  Pnt: TPoint;
begin
  if AUpdateText then
    FFinder.Editor.DoEventChange;

  with FFinder.Editor.Carets[0] do
  begin
    Pnt.X:= PosX;
    Pnt.Y:= PosY;
  end;

  FFinder.Editor.Update(AUpdateText);
  FFinder.Editor.DoGotoPos(
    Pnt,
    Point(-1, -1),
    UiOps.FindIndentHorz,
    UiOps.FindIndentVert,
    false,
    true
    );
  UpdateStatus;
end;

procedure TfmMain.DoFindCurrentWordOrSel(ANext: boolean; AWordOrSel: boolean);
var
  Str: atString;
  Ed: TATSynEdit;
  ok, bFlag: boolean;
  bSavedWords, bSavedRegex, bSavedCase: boolean;
begin
  Ed:= CurrentEditor;
  if Ed.Carets.Count>1 then exit;

  if AWordOrSel then
  begin
    Str:= Ed.TextCurrentWord;
    Ed.DoCommand(cCommand_SelectWords);
  end
  else
  begin
    Str:= Ed.TextSelected;
    if Str='' then
    begin
      Str:= Ed.TextCurrentWord;
      Ed.DoCommand(cCommand_SelectWords);
    end;
  end;
  if Str='' then exit;

  //SavedFind:= FFinder.StrFind; //don't save/restore
  bSavedWords:= FFinder.OptWords;
  bSavedRegex:= FFinder.OptRegex;
  bSavedCase:= FFinder.OptCase;

  FFinder.StrFind:= Str;
  FFinder.StrReplace:= '';
  FFinder.Editor:= Ed;

  FFinder.OptRegex:= false;
  FFinder.OptWords:= AWordOrSel;
  FFinder.OptFromCaret:= true;
  FFinder.OptBack:= not ANext;
  FFinder.OptInSelection:= false;

  case UiOps.FindSelCase of
    0: FFinder.OptCase:= false;
    1: FFinder.OptCase:= true;
    2:
      begin
        if Assigned(fmFind) then
          FFinder.OptCase:= fmFind.chkCase.Checked;
      end;
  end;

  ok:= FFinder.DoAction_FindOrReplace(false, false, false, bFlag);
  FinderUpdateEditor(false);
  DoFindResult(ok);

  //FFinder.StrFind:= SavedFind;
  FFinder.OptWords:= bSavedWords;
  FFinder.OptRegex:= bSavedRegex;
  FFinder.OptCase:= bSavedCase;
end;

procedure TfmMain.DoFind_ExpandSelectionToWord;
var
  Ed: TATSynEdit;
  Caret: TATCaretItem;
  X1, Y1, X2, Y2: integer;
  bSel, bFound: boolean;
  SavedFind: UnicodeString;
  bSavedWords,
  bSavedRegex,
  bSavedCase,
  bSavedFromCaret: boolean;
  ResBegin, ResEnd: TPoint;
begin
  Ed:= CurrentEditor;
  Caret:= Ed.Carets[Ed.Carets.Count-1];

  Caret.GetRange(X1, Y1, X2, Y2, bSel);
  //pos are sorted: (X1,Y1) <= (X2,Y2)

  if not bSel then
  begin
    Ed.DoSelect_Word(Point(X1, Y1), false);
  end
  else
  begin
    SavedFind:= FFinder.StrFind;
    bSavedWords:= FFinder.OptWords;
    bSavedRegex:= FFinder.OptRegex;
    bSavedCase:= FFinder.OptCase;
    bSavedFromCaret:= FFinder.OptFromCaret;

    FFinder.StrFind:= Ed.Strings.TextSubstring(X1, Y1, X2, Y2);
    FFinder.StrReplace:= '';
    FFinder.Editor:= Ed;

    FFinder.OptWords:= true;
    FFinder.OptRegex:= false;
    FFinder.OptCase:= true;
    FFinder.OptBack:= false;
    FFinder.OptFromCaret:= false;
    FFinder.OptInSelection:= false;

    bFound:= FFinder.DoAction_FindSimple(
      Point(X2, Y2),
      Point(Ed.Strings.LinesLen[Ed.Strings.Count-1], Ed.Strings.Count-1),
      ResBegin,
      ResEnd
      );
    DoFindResult(bFound);

    if bFound then
    begin
      Ed.Carets.Add(
        ResEnd.X, ResEnd.Y,
        ResBegin.X, ResBegin.Y
        );
      Ed.DoCommand(cCommand_ScrollToCaretBottom);
    end;

    FFinder.StrFind:= SavedFind;
    FFinder.OptWords:= bSavedWords;
    FFinder.OptRegex:= bSavedRegex;
    FFinder.OptCase:= bSavedCase;
    FFinder.OptFromCaret:= bSavedFromCaret;
  end;
end;


function TfmMain.DoFindOptionsToString: string;
begin
  Result:=
    //ignore OptBack
    IfThen(FFinder.OptCase, 'c')+
    IfThen(FFinder.OptRegex, 'r')+
    IfThen(FFinder.OptWords, 'w')+
    IfThen(FFinder.OptFromCaret, 'f')+
    IfThen(FFinder.OptInSelection, 's')+
    IfThen(FFinder.OptConfirmReplace, 'o')+
    IfThen(FFinder.OptWrapped, 'a');
end;

procedure TfmMain.DoFindOptionsFromString(const S: string);
begin
  FFinder.OptBack:= false; //ignore OptBack
  FFinder.OptCase:= Pos('c', S)>0;
  FFinder.OptRegex:= Pos('r', S)>0;
  FFinder.OptWords:= Pos('w', S)>0;
  FFinder.OptFromCaret:= Pos('f', S)>0;
  FFinder.OptInSelection:= Pos('s', S)>0;
  FFinder.OptConfirmReplace:= Pos('o', S)>0;
  FFinder.OptWrapped:= Pos('a', S)>0;
end;

procedure TfmMain.DoFindOptions_GetStrings(out AFind, AReplace: string);
begin
  AFind:= Utf8Encode(FFinder.StrFind);
  AReplace:= Utf8Encode(FFinder.StrReplace);
end;

function TfmMain.DoFindOptions_GetDict: PPyObject;
var
  oFind, oRep,
  bCase, bWord, bRegex, bCfm,
  bInSel, bWrap, bMulLine: PPyObject;
begin
  with GetPythonEngine do
  begin
    if not Assigned(FFinder) then
      exit(ReturnNone);

    oFind:= ReturnNone;
    oRep:= ReturnNone;
    bCase:= ReturnNone;
    bWord:= ReturnNone;
    bRegex:= ReturnNone;
    bCfm:= ReturnNone;
    bInSel:= ReturnNone;
    bWrap:= ReturnNone;
    bMulLine:= ReturnNone;

    if Assigned(fmFind) then
    begin
      oFind:= PyUnicode_FromWideString(fmFind.edFind.Text);
      oRep:= PyUnicode_FromWideString(fmFind.edRep.Text);
      bCase:= PyBool_FromLong(Ord(fmFind.chkCase.Checked));
      bWord:= PyBool_FromLong(Ord(fmFind.chkWords.Checked));
      bRegex:= PyBool_FromLong(Ord(fmFind.chkRegex.Checked));
      bCfm:= PyBool_FromLong(Ord(fmFind.chkConfirm.Checked));
      bInSel:= PyBool_FromLong(Ord(fmFind.chkInSel.Checked));
      bWrap:= PyBool_FromLong(Ord(fmFind.chkWrap.Checked));
      bMulLine:= PyBool_FromLong(Ord(fmFind.chkMulLine.Checked));
    end;

    Result:= Py_BuildValue('{sOsOsOsOsOsOsOsOsOsOsOsOsOsOsOsOsOsOsO}',
      'find', PyUnicode_FromWideString(FFinder.StrFind),
      'rep', PyUnicode_FromWideString(FFinder.StrReplace),

      'find_d', oFind,
      'rep_d', oRep,

      'op_case', PyBool_FromLong(Ord(FFinder.OptCase)),
      'op_back', PyBool_FromLong(Ord(FFinder.OptBack)),
      'op_word', PyBool_FromLong(Ord(FFinder.OptWords)),
      'op_regex', PyBool_FromLong(Ord(FFinder.OptRegex)),
      'op_cfm', PyBool_FromLong(Ord(FFinder.OptConfirmReplace)),
      'op_fromcaret', PyBool_FromLong(Ord(FFinder.OptFromCaret)),
      'op_insel', PyBool_FromLong(Ord(FFinder.OptInSelection)),
      'op_wrap', PyBool_FromLong(Ord(FFinder.OptWrapped)),

      'op_case_d', bCase,
      'op_word_d', bWord,
      'op_regex_d', bRegex,
      'op_cfm_d', bCfm,
      'op_insel_d', bInSel,
      'op_wrap_d', bWrap,
      'op_mulline_d', bMulLine
      );
  end;
end;


procedure TfmMain.DoFindOptions_ApplyDict(AText: string);
var
  SItem, SKey, SValue: string;
begin
  //text is '{key1:value1;key2:value2}' from to_str()
  if AText[1]='{' then
    AText:= Copy(AText, 2, Length(AText)-2);

  repeat
    SItem:= SGetItem(AText, #1);
    if SItem='' then Break;
    SKey:= SGetItem(SItem, ':');
    SValue:= SItem;
    SValue:= StringReplace(SValue, #2, ',', [rfReplaceAll]);

    case SKey of
      'find':
        FFinder.StrFind:= UTF8Decode(SValue);
      'rep':
        FFinder.StrReplace:= UTF8Decode(SValue);

      'find_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.edFind.Text:= UTF8Decode(SValue);
            fmFind.UpdateState;
            fmFind.DoOnChange;
          end;
        end;
      'rep_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.edRep.Text:= UTF8Decode(SValue);
            fmFind.UpdateState;
            fmFind.DoOnChange;
          end;
        end;

      'op_case':
        FFinder.OptCase:= AppStrToBool(SValue);
      'op_back':
        FFinder.OptBack:= AppStrToBool(SValue);
      'op_word':
        FFinder.OptWords:= AppStrToBool(SValue);
      'op_regex':
        FFinder.OptRegex:= AppStrToBool(SValue);
      'op_cfm':
        FFinder.OptConfirmReplace:= AppStrToBool(SValue);
      'op_fromcaret':
        FFinder.OptFromCaret:= AppStrToBool(SValue);
      'op_insel':
        FFinder.OptInSelection:= AppStrToBool(SValue);
      'op_wrap':
        FFinder.OptWrapped:= AppStrToBool(SValue);

      'op_case_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.chkCase.Checked:= AppStrToBool(SValue);
            fmFind.UpdateState;
            fmFind.DoOnChange;
          end;
        end;
      'op_word_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.chkWords.Checked:= AppStrToBool(SValue);
            fmFind.UpdateState;
            fmFind.DoOnChange;
          end;
        end;
      'op_regex_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.chkRegex.Checked:= AppStrToBool(SValue);
            fmFind.UpdateState;
            fmFind.DoOnChange;
          end;
        end;
      'op_cfm_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.chkConfirm.Checked:= AppStrToBool(SValue);
            fmFind.UpdateState;
            fmFind.DoOnChange;
          end;
        end;
      'op_insel_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.chkInSel.Checked:= AppStrToBool(SValue);
            fmFind.UpdateState;
            fmFind.DoOnChange;
          end;
        end;
      'op_wrap_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.chkWrap.Checked:= AppStrToBool(SValue);
            fmFind.UpdateState;
            fmFind.DoOnChange;
          end;
        end;
      'op_mulline_d':
        begin
          if Assigned(fmFind) then
          begin
            fmFind.IsMultiLine:= AppStrToBool(SValue);
            fmFind.DoOnChange;
          end;
        end;
    end;
  until false;
end;


procedure TfmMain.DoFindActionFromString(AStr: string);
var
  SAction, SFind, SRep, SOpt: string;
begin
  SAction:= SGetItem(AStr, #1);
  SFind:= SGetItem(AStr, #1);
  SRep:= SGetItem(AStr, #1);
  SOpt:= SGetItem(AStr, #1);

  FFinder.Editor:= CurrentEditor;
  FFinder.StrFind:= UTF8Decode(SFind);
  FFinder.StrReplace:= UTF8Decode(SRep);
  DoFindOptionsFromString(SOpt);
  FindDialogDone(nil{skip dialog}, SAction);
end;

procedure TfmMain.DoFindOptions_OnChange(Sender: TObject);
begin
  //options are used for some commands
  //apply options immediately, so user dont need to do a search to apply them
  with fmFind do
  begin
    FFinder.OptCase:= chkCase.Checked;
    FFinder.OptWords:= chkWords.Checked;
    FFinder.OptRegex:= chkRegex.Checked;
    FFinder.OptWrapped:= chkWrap.Checked;
    FFinder.OptInSelection:= chkInSel.Checked;
    FFinder.OptConfirmReplace:= chkConfirm.Checked;
  end;
end;


