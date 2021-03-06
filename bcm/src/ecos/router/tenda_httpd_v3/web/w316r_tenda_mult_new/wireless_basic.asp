<!DOCTYPE html>
<html> 
<head>
<meta charset="utf-8" />
<meta http-equiv="Pragma" content="no-cache" />
<title>Wireless Basic Settings</title>
<link rel="stylesheet" type="text/css" href="css/screen.css">
</head>
<body onLoad="initbasic();">
<form method=post name="wireless_basic" action="/goform/wirelessBasic" >
<!--<input type="hidden" id="rebootTag" name="rebootTag">-->
<input type="hidden" id="GO" name="GO" value="wireless_basic.asp">
<input type="hidden" name="bssid_num" value="1">
	<fieldset class="table-field">
		<legend>Wireless Basic Settings</legend>
		<div class="control-group">
			<label for="enablewireless" class="control-label">Enable Wireless</label>
			<div class="controls">
				<input type="hidden" name="enablewirelessEx" />
				<input type="checkbox" name="enablewireless"  value="1" id="enablewireless"onClick="onenablewirelesschange()">
			</div>
		</div>
		<div id="divwieless" style="clear:both"><!-- 为了消除ie7bug 添加clear:both属性 -->
			<div class="control-group"> 
				<label class="control-label">SSID(Network Name)</label>
				<div class="controls"><input type="text" name=ssid id=ssid size=20 maxlength=32 class="text" /></div>
			</div>
			<div class="control-group">
				<label class="control-label">Wireless Working Mode</label>
				<div class="controls">
					<label class="radio"><input name=WirelessT type=radio value=0 onClick="changeWds(0);" />Wireless Access Point(AP)</label>
					<label class="radio"><input name=WirelessT type=radio value=1 onClick="changeWds(1);" />WDS Bridge Mode</label>
				</div>
			</div>
			<div class="control-group">
				<label class="control-label">Network Mode</label>
				<div class="controls">
					<select name="wirelessmode" id="wirelessmode" size="1" onChange="wirelessModeChange()">
						<option value=0>11b/g mixed mode</option>
						<option value=1>11b mode</option>
						<option value=4>11g mode</option>
						<option value=9>11b/g/n mixed mode</option>
					</select>
				</div>
			</div>      
			<div class="control-group"> 
				<label class="control-label">SSID Broadcast</label>
				<div class="controls">
					<label class="radio"><input type=radio name=broadcastssid value="0" checked />Enable</label>
					<label class="radio"><input type=radio name=broadcastssid value="1" />Disable</label>
				</div>
			</div>
			<div id="div_11b_channel" name="div_11b_channel" class="control-group">
				<label class="control-label">Channel</label>
				<div class="controls">
					<div id="11b"></div>
				</div>
			</div>
			
			<div id="div_11n" class="content2" style="display:none">
				<div class="control-group">
					<label class="control-label">Channel Bandwidth</label>
					<div class="controls">
						<label class="radio"><input type="radio" name="n_bandwidth" value="0" onClick="Channel_BandWidth_onClick()" checked />20&nbsp;</label>
						<label class="radio"><input type="radio" name="n_bandwidth" value="1" onClick="Channel_BandWidth_onClick()" />20/40</label>
					</div>
				</div>
				<div class="control-group">
					<label class="control-label">Extension Channel</label>
					<div class="controls">
						<select id="n_extcha" name="n_extcha" size="1"></select>
					</div>
				</div>
			</div>
			
			<div id="div_wmm">
				<div class="control-group"> 
					<label class="control-label">WMM Capable</label>
					<div class="controls">
						<label class="radio"><input type=radio name=wmm_capable value="on" onClick="wmm_capable_enable_switch()" checked />Enable</label>
						<label class="radio"><input type=radio name=wmm_capable value="off" onClick="wmm_capable_enable_switch()" />Disable</label>
					</div>
				</div>
				<div id="div_apsd_capable" class="control-group">
					<label class="control-label">APSD Capable</label>
					<div class="controls">
						<label class="radio"><input type=radio name=apsd_capable value="on" />Enable</label>
						<label class="radio"><input type=radio name=apsd_capable value="off" checked />Disable</label>
					</div>
				</div>
			</div>
			
			<div id="wdsMode">
				<h2 class="legend">Wireless Working Mode: WDS(Repeater mode)</h2>	
				<table>
					<tr id="wds_mac_list_1" name="wds_mac_list_1">
						<td class="control-label">AP MAC Address</td>
						<td class="controls">
						 <input type=text name=wds_1 size=20 maxlength=17 class="text" value=""><input type="hidden" name="ssid_1"><input type="hidden" name="schannel_1"></td>
					</tr>
					<tr id="wds_mac_list_2" name="wds_mac_list_2">
						<td class="control-label">AP MAC Address</td>
						<td class="controls">
							<input type=text name="wds_2" size=20 maxlength=17 class="text" value="">
							<input type="hidden" name="ssid_2">
							<input type="hidden" name="schannel_2">
						</td>
					</tr>
					<tr id="wds_mac_list_3" name="wds_mac_list_3" style="display:none;">
						<td class="control-label">AP MAC Address</td>
						<td class="controls"><input type=text name=wds_3 size=20 maxlength=17 class="txt_border" value=""><input type="hidden" name="ssid_3"><input type="hidden" name="schannel_3">
						</td>
					</tr>
					<tr id="wds_mac_list_4" name="wds_mac_list_4" style="display:none;">
						<td class="control-label">AP MAC Address</td>
						<td class="controls"><input type=text name=wds_4 size=20 maxlength=17 class="txt_border" value=""><input type="hidden" name="ssid_4"><input type="hidden" name="schannel_4">
						</td>
					</tr>
				</table>
				<p class="text-red">note_WDS</p>
				<br>
				<input type="hidden" name="wds_list" value="1"> 
				<div class="btn-group"><input name="wlSurveyBtn" id="wlSurveyBtn" type="button" class="btn btn-small" onClick="closeSurvey()" value="Close Scan" /></div>
				<table id="wdsScanTab" class="table">
					<thead>
						<tr>
							<th width="10%">Select</th>
							<th width="20%">SSID</th>
							<th width="30%">MAC Address</th>
							<th width="10%">Channel</th>
							<th width="15%">Security</th>
							<th width="15%">Signal Strength</th>
						</tr>
					</thead>
					<tbody></tbody>
				</table>
			</div>
		</div>
	</fieldset>
    <div class="btn-group">
		<input type="button" class="btn" value="OK" onClick="preSubmit(document.wireless_basic)" />
			<input type="button" class="btn last" value="Cancel" onClick="init(document.wireless_basic)" />
	</div>
</form> 
<div id="save" class="none"></div>
<script src="lang/b28n.js" type="text/javascript"></script>
<script>
//handle translate
(function() {
	B.setTextDomain(["base", "wireless"]);
	B.translate();
})();
</script>
<script type="text/javascript" src="js/gozila.js"></script>
<script>
var max_wds_num = 2,
	request = GetReqObj(),
	http_request2 = null,
	ChannelList_24G = ["AutoSelect", "2412MHz (Channel 1)", "2417MHz (Channel 2)",
			"2422MHz (Channel 3)", "2427MHz (Channel 4)", "2432MHz (Channel 5)",
			"2437MHz (Channel 6)", "2442MHz (Channel 7)", "2447MHz (Channel 8)",
			"2452MHz (Channel 9)", "2457MHz (Channel 10)", "2462MHz (Channel 11)",
			"2462MHz (Channel 12)", "2472MHz (Channel 13)", "2484MHz (Channel 14)"],
	WirelessWorkMode,
	wdsList,
	PhyMode,
	broadcastssidEnable,
	channel_index,
	countrycode,
	ht_bw,
	ht_extcha,
	enable_wl,
	mode,
	wmmCapable,
	APSDCapable,
	SSID0,
	wireless11bchannels;

function init(){
	var wireContent = document.getElementById("divwieless"),
		wireEnable = document.getElementById("enablewireless"),
		WirelessTs = document.wireless_basic.WirelessT;
		
    if(WirelessWorkMode==0){
		WirelessTs[0].checked = true;
		changeWds(0);
	} else if (WirelessWorkMode==1) {
		WirelessTs[1].checked = true;
		changeWds(1);	 
	}
	initWds();
	if(enable_wl==1){
		wireEnable.checked=true;
    	wireContent.style.display="";
	} else {
		wireEnable.checked=false;
		wireContent.style.display="none";		
	}
	initValue();
	if(mode==1){
		document.getElementById("sz11bChannel").disabled=true;
		document.getElementById("enablewireless").disabled=true;
	}
}
function showChannel() {
	var idx,bstr;
	if(channel_index == 0) {
		bstr = '<select id="sz11bChannel" name="sz11bChannel" size="1" onChange="ChannelOnChange()"><option value=0 selected>AutoSelect</option>';
	} else {
		bstr = '<select id="sz11bChannel" name="sz11bChannel" size="1" onChange="ChannelOnChange()"><option value=0>AutoSelect</option>';
	}
	if(wireless11bchannels == 11) {
		for (idx = 0; idx < 11; idx++) {
			if(channel_index==idx  + 1) {
				bstr+='<option value='+(idx+1)+' selected>'+(2412+5*idx)+'MHz (Channel '+(idx+1)+')</option>';
			} else {
				bstr+='<option value='+(idx+1)+'>'+(2412+5*idx)+'MHz (Channel '+(idx+1)+')</option>';
			}
		}
	} else if(wireless11bchannels == 13) {
		for (idx = 0; idx < 13; idx++) {
			if(channel_index==idx  + 1) {
				bstr+='<option value='+(idx+1)+' selected>'+(2412+5*idx)+ 'MHz (Channel '+(idx+1)+')</option>';
			} else {
				bstr+='<option value='+(idx+1)+'>'+(2412+5*idx)+ 'MHz (Channel '+(idx+1)+')</option>';
			}
		}
	} else if (wireless11bchannels == 14) {
		for(idx = 0; idx < 13; idx++) {
			if(channel_index ==idx + 1) {
				bstr+='<option value='+(idx+1)+' selected>'+(2412+5*idx)+ 'MHz (Channel '+(idx+1)+')</option>';
			} else {
				bstr+='<option value='+(idx+1)+'>'+(2412+5*idx)+ 'MHz (Channel '+(idx+1)+')</option>';
			}
		}
		if(channel_index==14) {
			bstr=+'<option value=14 selected>2484MHz (Channel 14)</option>';
		} else {
			bstr=+'<option value=14>2484MHz (Channel 14)</option>';
		}
	}
	bstr+='</select>';
	document.getElementById("11b").innerHTML=bstr;
}
function initbasic() {
	http_request2 = GetReqObj();
	http_request2.onreadystatechange = alertContents2;
	http_request2.open('POST', "/goform/wirelessInitBasic", true);
	http_request2.send("something");
}
function alertContents2() {
	var str;
	if (http_request2.readyState == 4) {
		if (http_request2.status == 200) {
		 	str = http_request2.responseText.split("\r");
			WirelessWorkMode = str[0];//0
			wdsList = str[1];//
			PhyMode = str[2];//9
			broadcastssidEnable = str[3];//0
			channel_index = str[4];//0
			countrycode = str[5];//US
			ht_bw = str[6];//1
			ht_extcha = str[7];//none
			enable_wl = str[8];//1
			mode = str[9];//ap
			wmmCapable = str[10];//on
			APSDCapable = str[11];//off
			SSID0 = str[12];
			wireless11bchannels = str[13];//11
			wireless11gchannels = str[14];//11
			showChannel();
			init();
		}
	}
}
//++++++++++++
function initWds() {
	var wdslistArray;
	if (wdsList != "") {
		wdslistArray = wdsList.split(" ");
		for(i = 1; i <= wdslistArray.length; i++) {
			document.wireless_basic["wds_" + i].value = wdslistArray[i - 1];
		}
	}
	document.getElementById("wdsScanTab").style.display = "none";
	document.getElementById("wlSurveyBtn").value = _("Open Scan");
}

function closeSurvey(){
	var tbl = document.getElementById("wdsScanTab").style,
		code = "/goform/WDSScan";	
	if (tbl.display == "") {
		tbl.display = "none";
		document.getElementById("wlSurveyBtn").value= _("Open Scan");
	} else {
		document.body.className = "onwdsscan";
		tbl.display="";
		document.getElementById("wlSurveyBtn").value= _("Close Scan");
		request.open("GET", code, true);
		request.onreadystatechange = RequestRes;
		request.setRequestHeader("If-Modified-Since","0");
		request.send(null);
	}	
}

function RequestRes(){
	if (request.readyState == 4) {
		initScan(request.responseText);
		
		//reset this iframe height by call parent iframe function
		window.parent.reinitIframe();
	}
}
function initScan(scanInfo) {
	var str1, str, len, tbl, maxcell, nc, nr,
		j, i;		
		
	if (scanInfo != '') {
	    var iserror=scanInfo.split("\n");
		if(iserror[0]!="stanley") {
			str1 = scanInfo.split("\r");
			len = str1.length;
			document.getElementById("wdsScanTab").style.display = "";
			document.getElementById("wlSurveyBtn").value=_("Close Scan");			
			tbl = document.getElementById("wdsScanTab").getElementsByTagName('tbody')[0];	
			maxcell = tbl.rows.length;
			for (j = maxcell; j > 1; j --) {
				tbl.deleteRow(j - 1);
			}
			for (i = 0; i < len; i ++) {
				str = str1[i].split("\t");
				nr = document.createElement('tr');
				nc = document.createElement('td');
				nr.appendChild(nc);	
				nc.innerHTML = "<input type=\"radio\" name=\"wlsSlt\" value=\"radiobutton\" onclick=\"macAcc()\"/>";				
				nc=document.createElement('td');
				nr.appendChild(nc);
				nc.innerHTML = str[0];				
				nc=document.createElement('td');
				nr.appendChild(nc);
				nc.innerHTML = str[1];			
				nc=document.createElement('td');
				nr.appendChild(nc);
				nc.innerHTML = str[2];				
				nc=document.createElement('td');
				nr.appendChild(nc);
				nc.innerHTML = str[3];	
				nc = document.createElement('td');
				nr.appendChild(nc);
				nc.innerHTML = str[4];				
				nr.align = "center";
				tbl.appendChild(nr);
			}
		}
	} else {
		document.getElementById("wdsScanTab").style.display = "none";
		document.getElementById("wlSurveyBtn").value=_("Open Scan");
	}
}

function macAcc() {
	if(!confirm(_("Please click on OK to confirm that you want to connect to this AP!"))) {
		return ;
	}
	var tbl = document.getElementById("wdsScanTab");
	var doc = document.wireless_basic;
	var mac, sc, ssid, schannel;
	var maxcell = tbl.rows.length;	
	for (var r = maxcell; r > 1; r --) {
		if (maxcell == 2) {
			sc = doc.wlsSlt.checked;
		} else {
			sc = doc.wlsSlt[r - 2].checked;
		}
		if (sc) {
			mac = tbl.rows[r - 1].cells[2].innerHTML;
			ssid = tbl.rows[r - 1].cells[1].innerHTML;
			schannel = tbl.rows[r - 1].cells[3].innerHTML;
			if(checkRepeat((mac)) == false) {
				return;
			}
			for(var k=1;k<=max_wds_num;k++) {   
				if(doc.elements["wds_"+k].value == "") {
					doc.elements["wds_"+k].value = mac;
					doc.elements["ssid_"+k].value = decodeSSID(ssid);
					doc.elements["schannel_"+k].value = schannel;
					//把SSID 和信道改成和对方一样
					doc.elements["ssid"].value = decodeSSID(ssid);
					doc.elements["sz11bChannel"].value = schannel;
					ChannelOnChange();					
					return true;
				}
				if(k == max_wds_num) {
					alert(_("AP MAC enteries are full! Please remove the MAC enteries you don't need."));
					return ;
				}
			}
		}
	}
}

function checkRepeat(mac){
	for(var i=1;i<=max_wds_num;i++) {
		if(mac.toUpperCase() == document.forms[0].elements["wds_"+i].value.toUpperCase()) {
			alert(_("The address already exists"));
			return false;
		}
	}
}
function onenablewirelesschange() {
	if(document.getElementById("enablewireless").checked) {
		document.getElementById("divwieless").style.display="";
		
		//reset this iframe height by call parent iframe function
		window.parent.reinitIframe();
	} else {
		document.getElementById("divwieless").style.display="none";
	}
	
}

function insertExtChannelOption() {
	var wmode = document.wireless_basic.wirelessmode.options.selectedIndex,
		channel_index=Number(document.getElementById("sz11bChannel").selectedIndex);
	if ((wmode == 3)) { //11bgn
		var x = document.getElementById("n_extcha");
		var leng = document.getElementById("sz11bChannel").length;
		if(channel_index == 0){
			x.length=1;
			x.options[0].text = "Auto Select";
			x.options[0].value = 0;
		} else if((channel_index >=1) && (channel_index <= 4)) {
			x.length=1;
			x.options[0].text = ChannelList_24G[channel_index+4];
			x.options[0].value = 1;
		} else if ((channel_index >= 5) && (channel_index <= 7)) {
			x.length=2;
			x.options[0].text = ChannelList_24G[channel_index-4];
			x.options[0].value = 0;
			x.options[1].text = ChannelList_24G[channel_index+4];
			x.options[1].value = 1;
		} else if (channel_index>7 && channel_index<10) {
			if(leng>=14) {
				x.length=2;
				
				x.options[0].text = ChannelList_24G[channel_index-4];
				x.options[0].value = 0;
				x.options[1].text = ChannelList_24G[channel_index+4];
				x.options[1].value = 1;
			} else {
				x.length=1;
				x.options[0].text = ChannelList_24G[channel_index-4];
				x.options[0].value = 0;
			}
		} else {
			if(channel_index==10 && leng==15) {
				x.length=2;
				
				x.options[0].text = ChannelList_24G[channel_index-4];
				x.options[0].value = 0;
				x.options[1].text = ChannelList_24G[channel_index+4];
				x.options[1].value = 1;
			} else {
				x.length=1;
				x.options[0].text = ChannelList_24G[channel_index-4];
				x.options[0].value = 0;
			}
		}
	}
}

function ChannelOnChange() {
	if(document.wireless_basic.wirelessmode.options.selectedIndex==3) {
		Channel_BandWidth_onClick()
		insertExtChannelOption();
	}
}

function Channel_BandWidth_onClick() {
	if (document.wireless_basic.n_bandwidth[0].checked == true) {
		document.wireless_basic.n_extcha.disabled = true;
	} else {
		document.wireless_basic.n_extcha.disabled = false;
	}
}
function initValue() {
	var ssidArray,
		current_channel_length,
		doc=document.wireless_basic;

    document.getElementById("ssid").value = decodeSSID(SSID0);
	PhyMode = 1*PhyMode;
	if ((PhyMode == 0) || (PhyMode == 1) || (PhyMode == 4) || (PhyMode == 9)) {
		if (PhyMode == 0) {
			doc.wirelessmode.options.selectedIndex = 0;
		} else if (PhyMode == 1) {
			doc.wirelessmode.options.selectedIndex = 1;
		} else if (PhyMode == 4) {
			doc.wirelessmode.options.selectedIndex = 2;
		} else if (PhyMode == 9) {
			doc.wirelessmode.options.selectedIndex = 3;
			document.getElementById("div_11n").style.display = "";
			doc.n_bandwidth.disabled = false;
		}
	}
	if (wmmCapable == "on") {
		doc.wmm_capable[0].checked = true;
		document.getElementById("div_apsd_capable").style.display = "";
		//doc.apsd_capable.disabled = false;
	} else {
		doc.wmm_capable[1].checked = true;
		document.getElementById("div_apsd_capable").style.display = "none";
		//doc.apsd_capable.disabled = true;
	}
	if (APSDCapable == "on") {
		doc.apsd_capable[0].checked = true;
	} else {
		doc.apsd_capable[1].checked = true;
	}
	if (broadcastssidEnable == 0) {
		doc.broadcastssid[0].checked = true;
	} else {
		doc.broadcastssid[1].checked = true;
	}
	if (ht_bw == 0) {
		doc.n_bandwidth[0].checked = true;
		doc.n_extcha.disabled = true;
	} else {
		doc.n_bandwidth[1].checked = true;
		doc.n_extcha.disabled = false;
	}
	insertExtChannelOption();
	if(ht_extcha == 'upper') {
		doc.n_extcha.options.selectedIndex = 0;
	} else if(ht_extcha == 'lower' ) {
		if(channel_index>=1&&channel_index<=4) {
			doc.n_extcha.options.selectedIndex = 0;
		} else {
			doc.n_extcha.options.selectedIndex = 1;
		}
	} else {
		doc.n_extcha.options.selectedIndex = 0;
	}
}
function changeWds(x){
	if(x==0) {
		document.getElementById("wdsMode").style.display = "none";
	} else {
		document.getElementById("wdsMode").style.display = "";
	}
	
	//reset this iframe height by call parent iframe function
	window.parent.reinitIframe();
}
function wmm_capable_enable_switch(){
	if (document.wireless_basic.wmm_capable[0].checked == true) {
		document.getElementById("div_apsd_capable").style.display = "";
		document.wireless_basic.apsd_capable.disabled = false;
	} else {
		document.getElementById("div_apsd_capable").style.display = "none";
		document.wireless_basic.apsd_capable.disabled = true;
	}
}
function wirelessModeChange() {
	var wmode=document.wireless_basic.wirelessmode.options.selectedIndex;
	document.getElementById("div_11n").style.display = "none";
	document.wireless_basic.n_bandwidth.disabled = true;
	wmode = 1*wmode;
	if (wmode == 3) {
		document.getElementById("div_11n").style.display = "";
		document.wireless_basic.n_bandwidth.disabled = false;
		insertExtChannelOption();
	} else {
		document.getElementById("div_11n").style.display = "none";
		document.wireless_basic.n_bandwidth.disabled = true;
	}
	if(mode==1) {
		document.getElementById("sz11bChannel").disabled=true;
	}
}
function CheckValue() {
	var submit_ssid_num,
		doc =document.wireless_basic,
		//reSsid = /^[\w`~!@#$^*()\-+={}\[\]\|:'<>.?\/ ]+$/,

		sid = doc.ssid.value,
		all_wds_list = '',
		reMac = /([A-Fa-f0-9]{2}:){5}[A-Fa-f0-9]{2}/,
		macI,
		macJ;
		
	if(!checkSSID(sid)) {
		//alert(_("SSID can not contain comma, semicolon, double quotation marks, ampersand, percent and backslash!"));
		doc.ssid.focus();
		doc.ssid.select();
		return false;
	}
	if(sid!=decodeSSID(SSID0)){
		var con=confirm(_("The SSID (network name) will be changed to %s! Please reconnect to the new SSID!",[sid]));
		if(con==false){
			doc.ssid.value=decodeSSID(SSID0);
			return false;
		}
	}
	submit_ssid_num = 1;
	doc.bssid_num.value = submit_ssid_num;
	if (doc.WirelessT[1].checked==true) {
		
		for (i = 1; i <= max_wds_num; i++) {
			macI = document.wireless_basic["wds_" + i].value;
			
			if (macI === "") {
				continue;
			}
			if((macI.charAt(1) == "1") || (macI.charAt(1) == "3") ||
					(macI.charAt(1) == "5") || (macI.charAt(1) == "7") ||
					(macI.charAt(1) == "9") || (macI.charAt(1).toLowerCase() == "b")|| 
					(macI.charAt(1).toLowerCase() == "d") ||
					(macI.charAt(1).toLowerCase() == "f")){
      		  	alert(_("Please enter the MAC address of your wireless device or correct unicast MAC address!"));
       		 	return false;
   			}
		 
			for (j = 1; j < i; j++) {
				macJ = document.wireless_basic["wds_" + j].value;
				
				if(macI === macJ ){
					alert(_("Please enter a different MAC address."));
					return false;
				}
			}
			if (!reMac.test(macI)){
					alert(_("Please enter a valid MAC address!"));
					return false;
			} else {
				all_wds_list += macI;
				all_wds_list += ' ';
			}
		}
		if (all_wds_list == "") {
			alert(_("AP MAC address field should not be left empty!"));
			doc.wds_1.focus();
			doc.wds_1.select(); 
			return false;
		} else {
			doc.wds_list.value = all_wds_list;
		}
	}
	return true;
}

function preSubmit(f){  
	var sid = f.ssid.value,
		fm = document.forms[0];
	if(fm.elements["enablewireless"].checked) {
		fm.elements["enablewirelessEx"].value = "1";
	} else {
		fm.elements["enablewirelessEx"].value = "0";
	}		
	if (CheckValue()) { 
		f.submit();
		if(sid == decodeSSID(SSID0)){
			showSaveMassage();
		}
	}
}
</script>
</body>
</html>
