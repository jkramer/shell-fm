
-- Shell.FM control tool.
-- Copyright (C) 2008-2009 by Jonas Kramer.
-- Published under the terms of the GNU General Public License (GPL).

import Network.Socket
import System.Environment
import Data.List
import System.IO
import System.IO.Error

main = do
	args <- getArgs
	case args of
		[ "play", station ] -> playStation station
		[ "stop"          ] -> stopStation
		[ "skip"          ] -> skipTrack
		[ "next"          ] -> skipTrack
		[ "love"          ] -> loveTrack
		[ "ban"           ] -> banTrack
		[ "quit"          ] -> quit
		[ "pause"         ] -> pause
		[ "tag-artist", _ ] -> tagArtist $ tail args
		[ "tag-album", _  ] -> tagAlbum $ tail args
		[ "tag-track", _  ] -> tagAlbum $ tail args
		[ "info", format  ] -> formatString format
		[ "info"          ] -> formatString "%a - %t"
		[ "artist-tags"   ] -> artistTags
		[ "album-tags"    ] -> albumTags
		[ "track-tags"    ] -> trackTags
		[]                  -> formatString "Now playing \"%t\" by %a."
		_                   -> putStrLn "Please read the shell-fm manual for valid commands."


unixConnect = do
	socketPath <- getEnv "SHELLFM_UNIX_PATH"
	unix <- socket AF_UNIX Stream 0
	connect unix (SockAddrUnix socketPath)
	return unix


playStation station = do
	sendCommand $ "play " ++ (prefixedStation station)


stopStation = do sendCommand "stop"
skipTrack   = do sendCommand "skip"
loveTrack   = do sendCommand "love"
banTrack    = do sendCommand "ban"
quit        = do sendCommand "quit"
pause       = do sendCommand "pause"

artistTags  = do sendCommandWithReply $ "artist-tags"
albumTags   = do sendCommandWithReply $ "album-tags"
trackTags   = do sendCommandWithReply $ "track-tags"

tagArtist tagList = do
	tagCommand tagList "artist"

tagAlbum tagList = do
	tagCommand tagList "album"

tagTrack tagList = do
	tagCommand tagList "track"

tagCommand tagList item = do
	sendCommand $ "tag-" ++ item ++ (joinTags tagList)


formatString format = do
	sendCommandWithReply $ "info " ++ format


joinTags xs = join "," xs


join _ []     = ""
join _ (x:[]) = x
join s (x:xs) = x ++ s ++ (join s xs)


sendCommand command = do
	unix <- unixConnect
	result <- send unix (command ++ "\n")
	return ()


sendCommandWithReply command = do
	unix <- unixConnect
	result <- send unix (command ++ "\n")
	safeReceive unix
	return ()


safeReceive unix = do
	result <- try action
	putStrLn $ either handleError replyString result
	where
		action = recvLen unix 1024


handleError error = "No reply or error occured."
replyString (result, length) = result


prefixedStation station =
	if "lastfm://" `isPrefixOf` station
		then station
		else "lastfm://" ++ station

