# BL49SP  -  V0.0.3
Wideband Lambda Controller with Bosch CJ125 and LSU 4.9<br/>
Usable as a stand-alone controller or as a add-in for your own design. 2-layer pcb - single side soldered<br/>

20.04.2020 - Version 0.0.1: basic tests are done, seems to be in a working state<br/>

Version 0.0.1 - basic functions implemented: 0,5V - 4,5V equals AFR10-20<br/>

Version 0.0.2 - modifications for automated production process done - PCBs arrived - further tests needed<br/>

Version 0.0.2a - minor corrections on bottom silkscreen<br/>

Version 0.0.3 - minor corrections / pins added to connect OLED-Display<br/>

Update 04.09.2020 - Version 0.0.2: First tests have shown the finished lambda controller in an working state.<br/>
In idle mode Uout is 2,39V (Lambda 1,00).<br/>
In running mode we have a Uout of 4,56V at AFR20 and a Uout of 0,54V at AFR10.<br/>
All measurements have been done with a uncalibrated Multimeter and must be verified with a calibrated device.<br/>
Possibly there is some room for more finetuning or improvements, but until here all looks fine so far. <br/>
Current draw is only some milliAmps in idle mode, something around 1,7 A while heating and 0,7 A in running mode.<br/>

Update 17.09.2020 - Added some illustrations to the wiki how to connect the V0.0.2 board: https://github.com/oelprinz-org/BL49SP/wiki/Connecting-the-board-V0.0.2

V0.0.3 fabrication files for Jlcpcb: https://github.com/oelprinz-org/BL49SP/tree/master/fabrication/V0.0.3

not yet - Precompiled firmware files and fuse settings you can find at https://github.com/oelprinz-org/BL49SP/tree/master/software/V0.0.3/Arduino/BL49SP/Build<br/>

Interactive BOM for easier hand-soldering you can download from the github (BL49SP/hardware/V0.0.3/bom)
or access online here: http://oelprinz.org/products/BL49SP/hardware/V0.0.3/iBOM/ibom.html <br/>

Quick link to schematic: https://github.com/oelprinz-org/BL49SP/blob/master/hardware/V0.0.3/schematics.pdf <br/>


<p align="center">
  <img src="hardware/V0.0.3/top.png" width="350" title="Top Side">
  <img src="hardware/V0.0.3/bottom.png" width="350" alt="Bottom Side"><br/>
  <img src="hardware/V0.0.3/top_layer.png" width="350" title="Top Layer">
  <img src="hardware/V0.0.3/bottom_layer.png" width="350" alt="Bottom Layer">
  <img src="hardware/V0.0.3/assembly_V0.0.3.png" width="350" alt="Assembly">
  
</p>
