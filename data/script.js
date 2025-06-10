/*********************************************************************

       Programmable Electrical Circuit Testing Device

 *********************************************************************
   Thesis Identifier: #24130

   Supervisor:        Giakoumis Aggelos, ang1960@ihu.gr

   Assigned Student:  Kiose Valerio, iee2019070

   Institution:       International Hellenic University
 ******************************************************************
  Description:
        biri biri

   Microcontroller:   Espressif ESP32

   Compiler:          Arduino IDE

   Notes:
      - Put error checking in the database function
 ********************************************************************/

var Socket;
var table = document.getElementById("table_values");
var counter = 0;
//make a array with min and max!
let min_array = [2,1], max_array = [2,2], results_array = ["f","f"], values_array = [1.33,2.33], command_rules = [];

/**************************************************************************
  init()
    -Initial settings.
***************************************************************************/
function init() {
    Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
    //Process data sent from ESP side
    Socket.onmessage = function(event) {
        processMessage(event);
    };
}

/**************************************************************************
  runTest()
    -Collects min and max data and sends them to the ESP side so results 
can be calculated.
***************************************************************************/
function runTest() {
    for (let i = 0; i < 16; i++){
		var min = table.rows[i].cells[1].querySelector('input').value;
		var max = table.rows[i].cells[2].querySelector('input').value;
		if (min == "" && max == ""){

		}else{
			sendData(min, max);
		}
	}
}

/**************************************************************************
  sendData()
    -Sends min & max values to the ESP side of the programm.
	-Parameters:
min: minimum accepted voltage at a testpoint.
max: maximum accepted voltage at a testpoint.
***************************************************************************/
function sendData(min, max) {
    var msg = {value_min: min, value_max: max};
    Socket.send(JSON.stringify(msg)); 
}

/**************************************************************************
  processMessage()
    -Parses messages received from the ESP side, in this case we receive the 
results and values from the last test run of the device.
	-Parameters:
event: Holds info about who called processMessage and for what reason.
***************************************************************************/
function processMessage(event) {
	var obj = JSON.parse(event.data);
	results_array[counter] = parseInt(obj.result);
	values_array[counter] = parseFloat(obj.value);
	table.rows[counter].cells[3].textContent = values_array[counter];
	table.rows[counter].cells[4].textContent = results_array[counter];
	switch(results_array[counter]){
		case 0:
			table.rows[counter].cells[4].className = "failed";
			break;
		case 1:
			table.rows[counter].cells[4].className = "passed";
			break;
		default:
			table.rows[counter].cells[4].className = "unmeasured";
			break;
	}
	counter++;
	//rule check will be called here but you need to trigger it from ESP side
}

/*****************************************************************************
  appendCard()
    -Appends the GUI rule card, so the user can add more testpoints to a rule.
	-Parameters:
event: Holds info about who called processMessage and for what reason.
*****************************************************************************/
function appendCard(event){
    var parentDiv = event.target.parentNode;
	var newDropdown1 = document.createElement("select");
	var newDropdown2 = document.createElement("select");
	var newDropdown3 = document.createElement("select");
	var lastDiv = parentDiv.lastElementChild;
	newDropdown1.innerHTML = `
		<select>
			<option value="and">AND</option>
			<option value="or">OR</option>
		</select>
	`;
	newDropdown2.innerHTML = `
		<select>
			<option value="0">TP 1</option>
			<option value="1">TP 2</option>
			<option value="2">TP 3</option>
			<option value="3">TP 4</option>
			<option value="4">TP 5</option>
			<option value="5">TP 6</option>
			<option value="6">TP 7</option>
			<option value="7">TP 8</option>
			<option value="8">TP 9</option>
			<option value="9">TP 10</option>
			<option value="10">TP 11</option>
			<option value="11">TP 12</option>
			<option value="12">TP 13</option>
			<option value="13">TP 14</option>
			<option value="14">TP 15</option>
			<option value="15">TP 16</option>
		</select>
	`;

	newDropdown3.innerHTML = `
		<select>
			<option value="<">less than Min</option>
			<option value=">">more than Max</option>
			<option value="f">failed</option>
			<option value="p">passed</option>
		</select>
	`;

	parentDiv.insertBefore(newDropdown1, lastDiv);
	parentDiv.insertBefore(newDropdown2, lastDiv);
	parentDiv.insertBefore(newDropdown3, lastDiv);
}

/**************************************************************************
  addRule()
    -Parses the GUI rule card and makes a machine readable rule.
	-Parameters:
event: Holds info about who called processMessage and for what reason.
***************************************************************************/
function addRule(event){
	var string_command = "if(";
	var parentDiv = event.target.parentNode;
    var selects = parentDiv.querySelectorAll('select');
	for (var i = 0; i < selects.length; i++){
		switch (selects[i].value){
			case "<":
				string_command += ("<" + "min_array[" + selects[i-1].value + "]");
				break;
			case ">":
				string_command += (">" + "max_array[" + selects[i-1].value + "]");
				break;
			case "f":
				string_command += ('=="f"');
				break;
			case "p":
				string_command += ('=="p"');
				break;
			case "or":
				string_command += ("||");
				break;
			case "and":
				string_command += ("&&");
				break;
			default:
				if (selects[i+1].value == "f" || selects[i+1].value == "p"){
					string_command += "results_array[" + selects[i].value + "]";
				}else{
					string_command += "values_array[" + selects[i].value + "]";
				}
				break;
		}
	}
    string_command += ")rulePrint('" + parentDiv.querySelector('textarea').value + "');";
    console.log(string_command);
	command_rules[0] = string_command;

	var ruleHolder = document.getElementById("rule-div");
	var newRule = document.createElement("div");
	var rule = document.createElement("button");
	var deleteButton = document.createElement("button");
	newRule.classList.add("card");
	rule.textContent = string_command;
	deleteButton.textContent = "Delete";
	deleteButton.addEventListener("click", function() {
		newRule.remove();
	});
	newRule.appendChild(rule);
	newRule.appendChild(deleteButton);
	ruleHolder.appendChild(newRule);
}

/**************************************************************************
  ruleCheck()
    -Runs all the rules after measurements are taken.
***************************************************************************/
function ruleCheck(){
	for (var i = 0; i < command_rules.length; i++){
		eval(command_rules[i]);
	}
}

/**************************************************************************
  rulePrint()
    -Makes a GUI card with the error of the triggered rule.
	-Parameters:
message: String that holds the error message for the triggered rule.
***************************************************************************/
function rulePrint(message){
	var ruleHolder = document.getElementById("rule-print");
	var newRule = document.createElement("div");
	newRule.classList.add("card");
	newRule.textContent = message;
	ruleHolder.appendChild(newRule);
}
window.onload = function(event) { init(); }