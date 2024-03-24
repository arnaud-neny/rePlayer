Ken Silverman's Official Home Page: http://www.advsys.net/ken/

This folder contains a subset of the older (2006) code from the WinAmp plugin 
(http://www.advsys.net/ken/util/in_ken_src.zip - the .zip can also be found in the doc folder). 

I had originally given preference to this old version since it seemed to contain the least amount of x86 ASM 
code - minimizing the work needed to port it to C. 

After showing the initial version of my web player to Ken, he kindly offered to provide a properly done 
C replacement for the 'poor man's x86 code simulator approach' that I had originally used in the KDM 
player. Ken's new implementation immensely improves the readability of the code - benefiting readers 
like me who don't know much about x86 assembly. I therefore happily ditched my respective hack and 
replaced KDMENG.c with his shiny new version. It is basically the same file that can now (feb-19 2023) 
be found in http://www.advsys.net/ken/kdmsongs.zip (with some minor adaptations for use on the web, and 
cosmetic changes to align the APIs with those of the other two players) .

