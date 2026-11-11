-- Follow the authority's selected cue through the native music controller.
-- Updating a save field alone cannot clear a local Boss override or start a cue.
return function(valid,asset,resolve)
 local M={};local owned,wanted
 function M.capture(gi)
  local c=gi.MusicController
  return valid(c)and asset(c.CurrentPlayingTrack)or asset(gi.ActiveSaveSlot.PlayingMusicTrack)
 end
 function M.apply(gi,path)
  if not path or path==''then return false end
  local c=gi.MusicController;if not valid(c)then return false end
  if asset(c.CurrentPlayingTrack)==path and asset(c.CurrentTrackOverride)==path then return true end
  local cue=resolve(path);assert(cue:IsA('/Script/Engine.SoundBase'),'Invalid shared music cue')
  c:OverrideTrack(cue);owned=c;wanted=path;return true
 end
 function M.reset(clear)
  if clear and valid(owned)and asset(owned.CurrentTrackOverride)==wanted then owned:ClearOverrideTrack()end
  owned,wanted=nil,nil
 end
 return M
end
