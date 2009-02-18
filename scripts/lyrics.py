#!usr/bin/python


# Copyright (C) 2009 by Silviu Grijincu <silviu.grijincu@gmail.com>.
# Published under the terms of the GNU General Public License (GPL).

# INFO
# lyrics.py fetches lyrics from http://lyricwiki.org/. 
# The script is triggered by a key that is not already 
# in use by shell-fm and must be set. (See bellow)

# INSTALL
#	Save lyrics.py in the ~/.shell-fm/scripts/ directory.
#	(if the directories do not exist please create them)
#   To enable the plugin you must have a configuration file.
#	1.Create a file named shell-fm.rc in the ~/.shell-fm/ directory.
#	2.Put the following line in shell-fm.rc file :
#		key0x?? = /usr/bin/python ~/.shell-fm/scripts/lyrics.py "\"%a\"" "\"%t\""
#	4.Replace ?? with the ASCII hex code of the triggering key you wold like to use.
#		for example, to use ` (backquote) as a trigger replace ?? with 60 as bellow:
#		key0x60 = /usr/bin/python ~/.shell-fm/scripts/lyrics.py "\"%a\"" "\"%t\""



from urllib2 import *
from sys import argv



class LyricSource:
	"A lyric soruce"
	def __init__(self, artist, title):
		"init a Lyric source"
		self.artist = artist
		self.title  = title
	def get_lyrics(self):
		"get lyrics."
		return (False, "this is a dummy lyric source, no lyric can be found")

class DummyLyricSource(LyricSource):
	def __init__(self, artist, title):
		LyricSource.__init__(self, artist, title)
	def get_lyrics(self):
		return (False, "dummy error message from DummyLyricSource")

class LyricWiki(LyricSource): 
	def __init__(self, artist, title):
		LyricSource.__init__(self, artist, title)
		self.URL = 'http://lyricwiki.org/'
	
	def canonical(self, str):
		# remove start and end quotes
		str = str[1:len(str)-1]

		# capitalize every word after an ' ' and
		# replace ' ' with '_'
		return "_".join(map((lambda x : x.capitalize()), str.split(' ')))

	def get_url(self):
		return self.URL + self.canonical(self.artist) + ':' + self.canonical(self.title)

	def get_html(self, url):
		try:
			html = urlopen(url).read()
			return (True, html)
		except HTTPError, e:
			errmsg = 'The server couldn\'t fulfill the request for ' + url
			errmsg += 'Error code: ' + e.code
			return (False, errmsg)
		except URLError, e:
			errmsg = 'We failed to reach the server ' + url
			errmsg += 'Reason: ' + str(e.reason)
			return (False, errmsg)
		else:
			return (False, "unexpected path reached in LyricWiki.get_html")

	def filter_html_markers(self, html):
		rules = ( ('<br />', '\n'),
				  ('<br>', '\n'),
				  ('&gt;', '>'),
				  ('&lt;', '<'),
				  ('&nbsp;', ' '))
		for (x, y) in rules:
			html = html.replace(x, y)
		return html

	def parse_lyrics(self, html, url):
		instrumental = html.find('This song is an instrumental.')
		if instrumental <> -1:
			return (True, 'This song is instrumental.')

		start_marker = '<div class=\'lyricbox\' >'
		found = html.find(start_marker)
		if found == -1:
			return (False, 'Lyrics not found at ' + url + '.')
		
		start = found + len(start_marker)
		end = html.find('\n', start)
		lyrics = self.filter_html_markers(html[start:end])
		return (True, lyrics)

	def get_lyrics(self):
		url = self.get_url()
		(success, msg) = self.get_html(url)
		if success == False:
			return (False, msg)
		html = msg
		return self.parse_lyrics(html, url)

debug=False

def main(argv):
	artist = argv[1]
	title  = argv[2]
	# TODO: after adding a new LyricSouce class, append 
	# an object of that class here
	lyric_sources = (LyricWiki(artist, title), DummyLyricSource(artist, title))


	lyrics = map( (lambda x : x.get_lyrics()), lyric_sources)
	
	# print the first lyrics found
	for (success, lyric) in lyrics:
		if success == True:
			print lyric
			return
	
	if debug:
		# if no lyrics found, print all error messages
		for (success, errmsg) in lyrics:
			print errmsg
	else:
		print 'Lyrics not found.'

if __name__ == "__main__":
	main(argv)

