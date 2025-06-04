const PLACEHOLDER = "\
rooms:\n\
\tkitchen: [0.035, 2.285, 0]\n\
\tsecond_bedroom: [0.015, 11.555, 0]\n\
\tbedroom: [3.68, 11.045, 1.2]\n\
\tlivingroom: [3.59, 5.805, 1.2]\n\
\toffice: [10.48, 2.715, 0]\n\
roomplans:\n\
\t- name: kitchen\n\
\t\ty1: 0\n\
\t\tx1: 0\n\
\t\ty2: 4.29\n\
\t\tx2: 3.59\n\
\t- name: bathroom\n\
\t\ty1: 4.29\n\
\t\tx1: 0\n\
\t\ty2: 6.72\n\
\t\tx2: 2.36\n\
\t- name: toilet\n\
\t\ty1: 6.72\n\
\t\tx1: 0\n\
\t\ty2: 7.98\n\
\t\tx2: 2.36\n\
\t- name: second_bedroom\n\
\t\ty1: 7.98\n\
\t\tx1: 0\n\
\t\ty2: 12.06\n\
\t\tx2: 3.68\n\
\t- name: bedroom\n\
\t\ty1: 7.98\n\
\t\tx1: 3.68\n\
\t\ty2: 12.06\n\
\t\tx2: 7.6\n\
\t- name: entrance\n\
\t\ty1: 4.29\n\
\t\tx1: 2.36\n\
\t\ty2: 7.98\n\
\t\tx2: 3.59\n\
\t- name: entrance\n\
\t\ty1: 6.2\n\
\t\tx1: 3.59\n\
\t\ty2: 7.98\n\
\t\tx2: 7.6\n\
\t- name: livingroom\n\
\t\ty1: 1.37\n\
\t\tx1: 3.59\n\
\t\ty2: 6.2\n\
\t\tx2: 7.6\n\
\t- name: office\n\
\t\ty1: 0\n\
\t\tx1: 7.6\n\
\t\ty2: 4.92\n\
\t\tx2: 10.53";

// get references to the canvas and context
var canvas = document.getElementById("canvas");
var overlay = document.getElementById("overlay");
var ctx = canvas.getContext("2d");
var ctxo = overlay.getContext("2d");

var scrollOffsetX = 0;
var scrollOffsetY = 0;

var probeMode = false;
var probeRoom = -1;
var probePositionX = 0;
var probePositionY = 0;

var mqttInitData = {
    host: "",
    port: 1884,
    topic: "espresense/ips",
    username: "",
    password: ""
}

var savedUnit = window.localStorage.getItem("unit");
var currentUnit = savedUnit ?? "m";
var btn = document.querySelector(".unit-btn");
var label = btn.querySelector(".txt-icn");
label.innerHTML = currentUnit;

var convertedYAMLJSON;

var devices = [];
var showDevices = false;

var firstCoordinateOffsetX = 0;
var firstCoordinateOffsetY = 0;

var showESPCoverage = false;

// get stock container to pick element to clone and inject
var selfStorage = document.getElementById("stock");

// style the context
ctx.strokeStyle = "blue";
ctx.lineWidth = 3;
ctxo.strokeStyle = "blue";
ctxo.lineWidth = 3;

var exportedValue = "";
var showLabels = true;

// calculate where the canvas is on the window
// (used to help calculate mouseX/mouseY)
var $canvas = document.querySelector("#canvas");
var offsetX = $canvas.offsetLeft;
var offsetY = $canvas.offsetTop;
var scrollX = $canvas.scrollLeft;
var scrollY = $canvas.scrollTop;

// this flage is true when the user is dragging the mouse
var isDown = false;

// these vars will hold the starting mouse position
var startX;
var startY;

var prevStartX = 0;
var prevStartY = 0;

var prevWidth = 0;
var prevHeight = 0;

canvas.width = document.getElementById("canvasWrapper").getBoundingClientRect().width;
canvas.height = document.getElementById("canvasWrapper").getBoundingClientRect().height;

overlay.width = document.getElementById("canvasWrapper").getBoundingClientRect().width;
overlay.height = document.getElementById("canvasWrapper").getBoundingClientRect().height;

// resize canvas on window resize
window.addEventListener("resize", function(e) {
    canvas.width = document.getElementById("canvasWrapper").getBoundingClientRect().width;
    canvas.height = document.getElementById("canvasWrapper").getBoundingClientRect().height;

    overlay.width = document.getElementById("canvasWrapper").getBoundingClientRect().width;
    overlay.height = document.getElementById("canvasWrapper").getBoundingClientRect().height;
});

render();

// renders the canvas overlay (where rooms are drawn after you mouseup) and right list buttons
// you can choose not to render buttons and can pass an id to highlish the room on the canvas
function render(hightlightId = null, renderButtons = true) {
    var storage = window.localStorage.getItem("rooms");
    if (storage) {
        var jsonStorage = JSON.parse(storage);
        if (jsonStorage) {
            ctxo.clearRect(0, 0, overlay.width, overlay.height);
            // parse array and draw each rooms
            jsonStorage.rooms.forEach((room) => {
                // hightlight room or not
                if (hightlightId != null && hightlightId == room.id) {
                    ctxo.fillStyle = '#335bd07a';
                    ctxo.fillRect(room.zone.x, room.zone.y, room.zone.width, room.zone.height);
                } else {
                    if (probeMode && room.id != probeRoom) {
                        ctxo.fillStyle = '#0000007a';
                        ctxo.fillRect(room.zone.x, room.zone.y, room.zone.width, room.zone.height);
                    } else {
                        ctxo.strokeStyle = '#ffffff';
                        ctxo.strokeRect(room.zone.x, room.zone.y, room.zone.width, room.zone.height);
                    }

                }
                if (showLabels) {
                    // draw mesures
                    ctxo.font = "15px Helvetica";
                    ctxo.textAlign = "center";
                    ctxo.textBaseline = "middle";
                    if (hightlightId != null && hightlightId == room.id) {
                        ctxo.fillStyle = '#ffffff';
                    } else {
                        ctxo.fillStyle = '#ffffff';
                    }

                    if (!probeMode) {
                        ctxo.fillStyle = '#ffffff';
                        ctxo.fillText(room.name, room.text.width.x, room.text.width.y - 20);
                        ctxo.fillText(room.text.width.label+currentUnit, room.text.width.x, room.text.width.y);
                        ctxo.fillText(room.text.height.label+currentUnit, room.text.height.x, room.text.height.y);
                    }
                }

                room.probes.forEach(probe => {
                    ctxo.fillStyle = "#18b249";
                    ctxo.fillRect(probe.x, probe.y, probe.width, probe.height);

                    if (showESPCoverage) {
                        ctxo.beginPath();
                        ctxo.fillStyle = probe.color;
                        ctxo.arc(probe.x, probe.y, probe.coverage, 0, 2 * Math.PI);
                        ctxo.fill();
                        ctxo.closePath();
                    }
                });

                if (showDevices) {
                    var mqttKeys = Object.keys(devices);
                    mqttKeys.forEach(key => {
                        var deviceRoot = devices[key];
                        var device = deviceRoot.data;
                        if (!deviceRoot.hidden) {
                            ctxo.beginPath();
                            ctxo.fillStyle = deviceRoot.color ? deviceRoot.color : "#ffffff80";
                            ctxo.arc(device.x + firstCoordinateOffsetX, device.y + firstCoordinateOffsetY, 20, 0, 2 * Math.PI);
                            ctxo.fill();
                            ctxo.closePath();

                            ctxo.fillStyle = '#ffffff';
                            ctxo.fillText(device.name, (device.x + firstCoordinateOffsetX), (device.y + firstCoordinateOffsetY) + 35);
                        }
                    })
                }

                // if buttons have to be re-rendered, reset them based on localstorage
                if (renderButtons) {
                    addRoomItemToList(room.id, room);
                }
            });
        }
    }
}

function toggleLabels() {
    if (showLabels) {
        showLabels = false;
        document.querySelector(".labels").classList.remove("active");
    } else {
        showLabels = true;
        document.querySelector(".labels").classList.add("active");
    }
    render(null, false);
}

function getRandomColor() {
    var letters = '0123456789ABCDEF';
    var color = '#';
    for (var i = 0; i < 6; i++) {
        color += letters[Math.floor(Math.random() * 16)];
    }
    return color + "40";
}

// reset deletes the rooms in the local storage
// clear canvas overlay
function reset() {
    if (confirm('Are you sure you want to reset your floor plan ?')) {
        // Reset!
        window.localStorage.removeItem("rooms");
        ctxo.clearRect(0, 0, overlay.width, overlay.height);
        document.querySelector(".rooms").innerHTML = "";
    }
}

// click on the > button on a right room list
// toggles animation based on class
function roomInfo(id) {
    var menuRoom = document.querySelector("#room" + id);
    var button = menuRoom.querySelector("button.more-info");
    if (menuRoom.classList.contains("open")) {
        menuRoom.classList.remove("open");
        button.classList.remove("open");
        stopProbeMode(menuRoom.querySelector("button#probe"), id);
    } else {
        menuRoom.classList.add("open");
        button.classList.add("open");
    }
};

// delete a room from the localstorage and the menu
// render the canvas overlay
function deleteRoom(id) {
    if (confirm('Are you sure you want to delete this room ?')) {
        var jsonStorage = getRooms();
        // if localstorage has only one element, reset it to empty, else, filter out the one that fits the id
        if (jsonStorage.rooms.length <= 1) {
            jsonStorage.rooms = [];
        } else {
            jsonStorage.rooms = jsonStorage.rooms.filter(x => x.id !== id);
        }
        setRooms(jsonStorage);
        document.getElementById("room" + id).remove();
        // clear canvas overlay then re-render canvas overlay
        ctxo.clearRect(0, 0, overlay.width, overlay.height);
        render(null, false);
    }
}

// get cursor postion on canvas
function getMousePos(canvas, evt) {
    var rect = canvas.getBoundingClientRect();
    return {
        x: evt.clientX - rect.left,
        y: evt.clientY - rect.top
    };
}

function handleMouseDown(e) {
    e.preventDefault();
    e.stopPropagation();

    if (!probeMode) {
        // save the starting x/y of the rectangle
        startX = parseInt(cursorPositionX - offsetX);
        startY = parseInt(cursorPositionY - offsetY);

        // set a flag indicating the drag has begun
        isDown = true;
    } else {
        setProbe(probePositionX, probePositionY, probeRoom);
    }
}

function uuidv4() {
    return ([1e7] + -1e3 + -4e3 + -8e3 + -1e11).replace(/[018]/g, c =>
        (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
    );
}

function setProbe(x, y, roomId) {
    var data = getRooms();
    var room = data.rooms.find(x => x.id == roomId);
    if (!room.probes) {
        room.probes = [];
    }
    var probeData = {
        id: uuidv4(),
        name: room.name + "-" + probe.id,
        color: getRandomColor(),
        coverage: 500,
        x: x,
        y: y,
        z: 0,
        width: 5,
        height: 15,
    }
    room.probes.push(probeData);
    setRooms(data);
    var item = document.querySelector(".rooms #room" + roomId);
    var probeList = item.querySelector(".probe-list");
    var probeButton = document.createElement("button");
    probeButton.classList.add("probe-item");
    probeButton.onclick = function() {
        openProbeModal(roomId, probeData.id);
    }
    probeList.append(probeButton);
}

// get rooms from local storage or return an empty array
// converts string data to object
function getRooms() {
    var dataString = window.localStorage.getItem('rooms');
    var data;
    if (!dataString) {
        data = {};
        data.rooms = [];
    } else {
        data = JSON.parse(dataString);
    }
    return data;
}

// update localstorage
// converts objects to string
function setRooms(data) {
    window.localStorage.setItem('rooms', JSON.stringify(data));
}

// export rooms data to an ESPresenseIPS yaml format
function exportToYaml() {
    var jsonStorage = getRooms();
    var infoTxt = document.querySelector(".info-ft");
    if (currentUnit == "m") {
        infoTxt.hidden = true;
    } else {
        infoTxt.hidden = false;
    }
    //get most left x value
    if (jsonStorage.rooms.length) {
        var mostLeftRoom = jsonStorage.rooms.reduce(function(prev, curr) {
            return prev.zone.x <= curr.zone.x ? prev : curr;
        });

        // get highest y value
        var mostBottomRoom = jsonStorage.rooms.reduce(function(prev, curr) {
            return (prev.zone.y + prev.zone.height) >= (curr.zone.y + curr.zone.height) ? prev : curr;
        });

        // create an offseter
        firstCoordinateOffsetY = mostBottomRoom.zone.y + mostBottomRoom.zone.height;
        firstCoordinateOffsetX = mostLeftRoom.zone.x;

        var data = {
            rooms: [],
            roomplans: []
        }
        jsonStorage.rooms.forEach((room) => {
            // first coordinate is room x minus firstCoordinateOffsetX, room y minus firstCoordinateOffsetY
            // second coordinat is (room x minus firstCoordinateOffsetX) plus room width, (room y minus firstCoordinateOffsetY) plus room height
            //console.log(room.id, "x: " + (room.zone.x - firstCoordinateOffsetX) + ", y: " + (room.zone.y - firstCoordinateOffsetY));
            //console.log(room.id, "x: " + ((room.zone.x - firstCoordinateOffsetX) + room.zone.width) + ", y: " + ((room.zone.y - firstCoordinateOffsetY) + room.zone.height));
            var y1 = currentUnit == "m" ? Math.abs(room.zone.y - firstCoordinateOffsetY) : Math.abs(room.zone.y - firstCoordinateOffsetY)/3.2808;
            var x1 = currentUnit == "m" ? Math.abs(room.zone.x - firstCoordinateOffsetX) : Math.abs(room.zone.x - firstCoordinateOffsetX)/3.2808;
            var y2 = currentUnit == "m" ? Math.abs((room.zone.y - firstCoordinateOffsetY) + room.zone.height) : Math.abs((room.zone.y - firstCoordinateOffsetY) + room.zone.height)/3.2808;
            var x2 = currentUnit == "m" ? Math.abs((room.zone.x - firstCoordinateOffsetX) + room.zone.width) : Math.abs((room.zone.x - firstCoordinateOffsetX) + room.zone.width)/3.2808;
            var objRoom = {
                name: room.name ? room.name : "Room" + room.id,
                y1,
                x1,
                y2,
                x2
            }
            data.roomplans.push(objRoom);
        })

        var probesList = [];
        jsonStorage.rooms.forEach(room => {
            if (room.probes.length) {
                var data = {
                    roomName: room.name ? room.name : "Room" + room.id,
                    probes: room.probes,
                }
                probesList.push(data);
            }
        });

        data.rooms = probesList;

        exportedValue = objToYaml(data);
        var modal = document.querySelector(".yaml-export");
        modal.querySelector(".text").innerHTML = exportedValue.replaceAll("\n", "<br />").replaceAll("\t", "&nbsp;&nbsp;");
        modal.classList.add("visible");
        console.log(exportedValue);
    }
}

function yamlToJsonEXP() {
    alert("This is an experimental feature, the conversion agorithm is an abomination and relies on a strict syntax. There will be no support on this feature, you can tweak it yourself if you want");
    var modal = document.querySelector(".yaml-to-json-modal");
    modal.classList.add("visible");
    document.querySelector(".yaml-data").placeholder = PLACEHOLDER;
}

function closeModal(id) {
    var modal = document.querySelector(id);
    modal.classList.remove("visible");
    if (id == ".probe-modal") {
        activeProbeModalData.roomId = -1;
        activeProbeModalData.probe = null;
    }
}

function copyYaml() {
    navigator.clipboard.writeText(exportedValue.replaceAll("\t", "  "));
}

function objToYaml(obj) {
    var string = "rooms: \n"
    obj.roomplans.forEach(obj => {
        string += "\t- name: " + obj.name + "\n";
        string += "\t\tpoints:" + "\n";
        string += "\t\t\t- [ " + (Math.round(obj.x1)/100) + ", " + (Math.round(obj.y1)/100)   +"]\n";
        string += "\t\t\t- [ " + (Math.round(obj.x2)/100) + ", " + (Math.round(obj.y1)/100)   +"]\n";
        string += "\t\t\t- [ " + (Math.round(obj.x2)/100) + ", " + (Math.round(obj.y2)/100)   +"]\n";
        string += "\t\t\t- [ " + (Math.round(obj.x1)/100) + ", " + (Math.round(obj.y2)/100)   +"]\n";
        string += "\t\t\t- [ " + (Math.round(obj.x1)/100) + ", " + (Math.round(obj.y1)/100)   +"]\n";
    });

    string += "nodes: \n";
    obj.rooms.forEach(probesData => {
        probesData.probes.forEach(probe => {
            var x = currentUnit == "m" ? Math.abs(probe.x - firstCoordinateOffsetX) : Math.abs(probe.x - firstCoordinateOffsetX)/3.2808;
            var y =  currentUnit == "m" ? Math.abs(probe.y - firstCoordinateOffsetY) : Math.abs(probe.y - firstCoordinateOffsetY)/3.2808;
            string += "\t- name: " + probe.name +"\n";
            string += "\t\tpoint: [" + (Math.round(x)/100) + "," + (Math.round(y) / 100) + ", 0]\n";
            string += "\t\tfloors: [\"first\"]\n";
        });
    });
    return string;
}

// when user lets go of mouse click //finises drawing the room//
function handleMouseUp(e) {
    e.preventDefault();
    e.stopPropagation();

    if (!probeMode) {
        if (prevWidth && prevHeight) {
            var data = getRooms();
            // generate a room object and set its data
            var valueX = prevWidth < 0 ? prevStartX + prevWidth : prevStartX;
            var valueY = prevHeight < 0 ? prevStartY + prevHeight : prevStartY;
            var valueW = Math.abs(prevWidth);
            var valueH = Math.abs(prevHeight);

            var room = {
                name: "",
                id: data.rooms.length ? data.rooms[data.rooms.length - 1].id + 1 : 1,
                zone: {
                    x: valueX,
                    y: valueY,
                    width: valueW,
                    height: valueH,
                },
                text: {
                    width: {
                        label: 'width : ' + Math.abs(prevWidth / 100),
                        x: (prevStartX + (prevWidth / 2)),
                        y: (prevStartY + (prevHeight / 2)) - 10,
                    },
                    height: {
                        label: 'height : ' + Math.abs(prevHeight / 100),
                        x: (prevStartX + (prevWidth / 2)),
                        y: (prevStartY + (prevHeight / 2)) + 10,
                    }
                },
                probes: []
            };
            // add room to rooms and save to local storage
            data.rooms.push(room);
            setRooms(data);
            // add a new room in the list
            addRoomItemToList(room.id, room);

            // the drag is over, clear the dragging flag
            isDown = false;
            ctxo.clearRect(0, 0, overlay.width, overlay.height);

            render(null, false);
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            prevWidth = 0;
            prevHeight = 0;
        }
    }
}

function hightlightRoom(id) {
    var rooms = document.querySelector(".rooms");
    var theRoom = rooms.querySelector("#room" + id);
    theRoom.style.backgroundColor = "#335bd07a";
    ctxo.clearRect(0, 0, overlay.width, overlay.height);
    render(id, false);
}

function unhightlightRoom(id) {
    var rooms = document.querySelector(".rooms");
    var theRoom = rooms.querySelector("#room" + id);
    theRoom.style.backgroundColor = "#4b4b4b";
    ctxo.clearRect(0, 0, overlay.width, overlay.height);
    render(null, false);
}

function setRoomName(id, name) {
    var jsonStorage = getRooms();
    var room = jsonStorage.rooms.find(x => x.id === id);
    room.name = name;
    setRooms(jsonStorage);
    document.querySelector("#room" + id).querySelector(".label").innerHTML = name;
    render(null, false);
}

function stopProbeMode(button, id) {
    button.classList.remove("active");
    var icon = button.querySelector("i");
    icon.classList.add("fa-plus");
    icon.classList.remove("fa-check");
    probeMode = false;
    probeRoom = -1;
    threshold = 50;
    render(null, false);
}

function startProbeMode(button, id) {
    var buttons = document.querySelector(".rooms").querySelectorAll(".add-probe-btn");
    buttons.forEach(btn => {
        btn.classList.remove("active");
        var icon = btn.querySelector("i");
        icon.classList.add("fa-plus");
        icon.classList.remove("fa-check");
    });
    button.classList.add("active");
    var icon = button.querySelector("i");
    icon.classList.remove("fa-plus");
    icon.classList.add("fa-check");
    probeMode = true;
    probeRoom = id;
    threshold = 1;
    render(null, false);
}

function toggleProbeMode(id, button) {
    if (button.classList.contains("active")) {
        stopProbeMode(button, id);
    } else {
        startProbeMode(button, id);
    }
}


var activeProbeModalData = {
    roomId: -1,
    probe: null,
};

function openProbeModal(id, probeId) {
    var jsonStorage = getRooms();

    //get most left x value
    if (jsonStorage.rooms.length) {
        var mostLeftRoom = jsonStorage.rooms.reduce(function(prev, curr) {
            return prev.zone.x <= curr.zone.x ? prev : curr;
        });

        // get highest y value
        var mostTopRoom = jsonStorage.rooms.reduce(function(prev, curr) {
            return prev.zone.y <= curr.zone.y ? prev : curr;
        });

        // create an offseter
        firstCoordinateOffsetY = mostTopRoom.zone.y;
        firstCoordinateOffsetX = mostLeftRoom.zone.x;
    }
    activeProbeModalData.roomId = id;
    var room = getRoom(id);
    var probe = room.probes.find(x => x.id === probeId);
    activeProbeModalData.probe = probe;
    var modal = document.querySelector(".probe-modal");
    var x = (Math.abs(probe.x - firstCoordinateOffsetX));
    var y = (Math.abs(probe.y - firstCoordinateOffsetY));
    modal.querySelector(".probe-name").value = probe.name;
    modal.querySelector(".probe-x").value = x;
    modal.querySelector(".probe-y").value = y;
    modal.querySelector(".probe-z").value = probe.z ? probe.z : 0;
    var code = (room.name ? room.name : "Room" + room.id) + ": [" + (x / 100) + ", " + (y / 100) + ", " + (probe.z / 100) + "]";
    document.querySelector(".code-area").innerHTML = code;

    modal.querySelector(".coverage").value = probe.coverage;
    modal.querySelector(".coverage-color").value = probe.color.slice(0, -2);

    modal.classList.add("visible");
}



function deleteProbe() {
    var data = getRooms();
    var room = data.rooms.find(x => x.id == activeProbeModalData.roomId);
    room.probes = room.probes.filter(x => x.id !== activeProbeModalData.probe.id);
    setRooms(data);
    document.querySelector(".rooms").innerHTML = "";
    render(null, true);
    closeModal(".probe-modal");
}

function saveProbe() {
    var valueZ = document.querySelector(".probe-z").value;
    var coverageValue = document.querySelector(".coverage").value;
    var coverageColor = document.querySelector(".coverage-color").value + "40";
    var name = document.querySelector(".probe-name").value;
    var data = getRooms();
    var room = data.rooms.find(x => x.id == activeProbeModalData.roomId);
    var probe = room.probes.find(x => x.id == activeProbeModalData.probe.id);
    probe.z = parseInt(valueZ);
    probe.coverage = coverageValue;
    probe.color = coverageColor;
    probe.name = name;
    setRooms(data);
    closeModal(".probe-modal");
    render(null, false);
}

function addRoomItemToList(id, room) {
    var storageRoom = selfStorage.querySelector(".room").cloneNode(true);
    storageRoom.id = "room" + id;
    storageRoom.onmouseover = function() {
        hightlightRoom(id);
    }
    storageRoom.onmouseout = function() {
        unhightlightRoom(id);
    }
    var button = storageRoom.querySelector("button#delete");
    button.onclick = function() {
        deleteRoom(id);
    }
    var input = storageRoom.querySelector("input");
    input.value = room.name ? room.name : "";
    input.addEventListener("keyup", (event) => {
        setRoomName(id, event.target.value);
    });

    var probeCont = storageRoom.querySelector(".probe-list");
    room.probes.forEach(probe => {
        var probeButton = document.createElement("button");
        probeButton.classList.add("probe-item");
        probeButton.onclick = function() {
            openProbeModal(id, probe.id);
        }
        probeCont.append(probeButton);
    });

    var probeAdd = storageRoom.querySelector("button#probe");
    probeAdd.onclick = function() {
        toggleProbeMode(id, probeAdd);
    }
    storageRoom.querySelector(".label").innerHTML = room.name ? room.name : "[" + id + "] Room";
    var mesures = storageRoom.querySelector(".mesures");
    mesures.querySelector(".width").innerHTML = room.text.width.label + currentUnit;
    mesures.querySelector(".height").innerHTML = room.text.height.label + currentUnit;
    var buttonMore = storageRoom.querySelector("button#more");
    buttonMore.onclick = function() {
        roomInfo(id);
    }
    document.querySelector(".rooms").append(storageRoom);
}

function handleMouseOut(e) {
    e.preventDefault();
    e.stopPropagation();

    // the drag is over, clear the dragging flag
    isDown = false;
}

function handleScroll(e) {
    // console.log(e);
    // console.log("Scroll left", e.ClientX);
    // console.log("Scroll top", e.ClientY);
    scrollOffsetX = -e.deltaX;
    scrollOffsetY = -e.deltaY;
    var jsonStorage = getRooms();
    if (jsonStorage.rooms.length) {
        var mostLeftRoom = jsonStorage.rooms.reduce(function(prev, curr) {
            return prev.zone.x <= curr.zone.x ? prev : curr;
        });

        // get highest y value
        var mostTopRoom = jsonStorage.rooms.reduce(function(prev, curr) {
            return prev.zone.y <= curr.zone.y ? prev : curr;
        });

        // create an offseter
        firstCoordinateOffsetY = mostTopRoom.zone.y;
        firstCoordinateOffsetX = mostLeftRoom.zone.x;
    }

    updateRoomsPosition(scrollOffsetX, scrollOffsetY);
    checkIfAllRoomsOutOfScreen();
    render(null, false);
}

function checkIfAllRoomsOutOfScreen() {
    var data = getRooms();
    var leftLimit = 0;
    var rightLimit = canvas.width;
    var topLimit = 0;
    var bottomLimit = canvas.height;

    var atleastOneIsVisible = false;
    var directions = [];
    for (let index = 0; index < data.rooms.length; index++) {
        const room = data.rooms[index];
        if (room.zone.x + room.zone.width > leftLimit &&
            room.zone.x < rightLimit &&
            room.zone.y + room.zone.height > topLimit &&
            room.zone.y < bottomLimit) {
            atleastOneIsVisible = true;
            break;
        } else {
            if (room.zone.x + room.zone.width < leftLimit) {
                directions.push("left");
            } else if (room.zone.x > rightLimit) {
                directions.push("rightt");
            } else if (room.zone.y + room.zone.height < topLimit) {
                directions.push("top");
            } else if (room.zone.y > bottomLimit) {
                directions.push("bottom");
            }
        }
    }

    var indicators = document.querySelectorAll(".indicator");
    indicators.forEach(indicator => {
        indicator.classList.remove("visible");
    });
    if (!atleastOneIsVisible) {
        directions = [...new Set(directions)];
        directions.forEach(direction => {
            var indic = document.querySelector(".indicator." + direction);
            if (indic) {
                indic.classList.add("visible");
            }
        })
    }
}

function updateRoomsPosition(scrollOffsetX, scrollOffsetY) {
    var data = getRooms();
    data.rooms.forEach(function(room) {
        room.zone.x = room.zone.x + scrollOffsetX;
        room.zone.y = room.zone.y + scrollOffsetY;
        room.text.width.x = room.text.width.x + scrollOffsetX;
        room.text.width.y = room.text.width.y + scrollOffsetY;
        room.text.height.x = room.text.height.x + scrollOffsetX;
        room.text.height.y = room.text.height.y + scrollOffsetY;
        room.probes.forEach(function(probe) {
            probe.x = probe.x + scrollOffsetX;
            probe.y = probe.y + scrollOffsetY;
        })
    })
    setRooms(data);
}

var threshold = 50;
var cursorPositionX = 0;
var cursorPositionY = 0;

function addText(startX, startY, width, height, value, baseLine = "middle") {
    ctx.font = "15px Helvetica";
    ctx.textAlign = "center";
    ctx.textBaseline = baseLine;
    ctx.fillStyle = "#ffffff";
    ctx.fillText(value, (startX + (width / 2)), (startY + (height / 2)) - 10);
}

function handleMouseMove(e) {
    e.preventDefault();
    e.stopPropagation();
    //console.log(pos);

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    var pos = getMousePos(canvas, e);
    ctx.beginPath();
    cursorPositionX = pos.x;
    cursorPositionY = pos.y;
    var data = getRooms();
    if (!probeMode) {
        data.rooms.forEach((room) => {
            if (
                (room.zone.x <= pos.x + threshold && room.zone.x >= pos.x - threshold) &&
                (pos.y >= room.zone.y && pos.y <= room.zone.y + room.zone.height)
            ) {
                if (room.zone.x <= pos.x + threshold && room.zone.x >= pos.x) {
                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(room.zone.x - 15, room.zone.y);
                    ctx.lineTo(room.zone.x - 15, pos.y);
                    ctx.stroke();
                    ctx.closePath();
                    addText(room.zone.x - 50, room.zone.y, 1, (pos.y - room.zone.y), ((pos.y - room.zone.y) / 100) + currentUnit, "right");

                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(room.zone.x, pos.y);
                    ctx.lineTo(room.zone.x - 15, pos.y);
                    ctx.stroke();
                    ctx.closePath();

                    ctx.beginPath();
                    ctx.moveTo(room.zone.x - 15, room.zone.y + room.zone.height);
                    ctx.lineTo(room.zone.x - 15, pos.y);
                    ctx.stroke();
                    ctx.closePath();
                    addText(room.zone.x - 50, pos.y, 1, ((room.zone.y + room.zone.height) - pos.y), (((room.zone.y + room.zone.height) - pos.y) / 100) + currentUnit, "right");
                }
                cursorPositionX = room.zone.x;
            } else if (
                (room.zone.x + room.zone.width <= pos.x + threshold && room.zone.x + room.zone.width >= pos.x - threshold) &&
                (pos.y >= room.zone.y && pos.y <= room.zone.y + room.zone.height)
            ) {
                if (room.zone.x + room.zone.width <= pos.x && room.zone.x + room.zone.width >= pos.x - threshold) {
                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo((room.zone.x + room.zone.width) + 15, room.zone.y);
                    ctx.lineTo((room.zone.x + room.zone.width) + 15, pos.y);
                    ctx.stroke();
                    ctx.closePath();
                    addText((room.zone.x + room.zone.width) + 50, room.zone.y, 1, (pos.y - room.zone.y), ((pos.y - room.zone.y) / 100) + currentUnit, "left");

                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(room.zone.x + room.zone.width, pos.y);
                    ctx.lineTo(room.zone.x + room.zone.width + 15, pos.y);
                    ctx.stroke();
                    ctx.closePath();

                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo((room.zone.x + room.zone.width) + 15, room.zone.y + room.zone.height);
                    ctx.lineTo((room.zone.x + room.zone.width) + 15, pos.y);
                    ctx.stroke();
                    ctx.closePath();
                    addText((room.zone.x + room.zone.width) + 50, pos.y, 1, ((room.zone.y + room.zone.height) - pos.y), (((room.zone.y + room.zone.height) - pos.y) / 100) + currentUnit, "left");
                }
                cursorPositionX = room.zone.x + room.zone.width;
            } else if (
                (room.zone.y <= pos.y + threshold && room.zone.y >= pos.y - threshold) &&
                (pos.x >= room.zone.x && pos.x <= room.zone.x + room.zone.width)
            ) {
                if (room.zone.y <= pos.y + threshold && room.zone.y >= pos.y) {
                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(room.zone.x, room.zone.y - 15);
                    ctx.lineTo(pos.x, room.zone.y - 15);
                    ctx.stroke();
                    ctx.closePath();
                    addText(room.zone.x, room.zone.y - 15, (pos.x - room.zone.x), 1, ((pos.x - room.zone.x) / 100) + currentUnit, "right");

                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(pos.x, room.zone.y);
                    ctx.lineTo(pos.x, room.zone.y - 15);
                    ctx.stroke();
                    ctx.closePath();

                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(room.zone.x + room.zone.width, room.zone.y - 15);
                    ctx.lineTo(pos.x, room.zone.y - 15);
                    ctx.stroke();
                    ctx.closePath();
                    addText(pos.x, room.zone.y - 15, ((room.zone.x + room.zone.width) - pos.x), 1, ((room.zone.x + room.zone.width) - pos.x) + currentUnit, "right");
                }
                cursorPositionY = room.zone.y;
            } else if (
                (room.zone.y + room.zone.height <= pos.y + threshold && room.zone.y + room.zone.height >= pos.y - threshold) &&
                (pos.x >= room.zone.x && pos.x <= room.zone.x + room.zone.width)
            ) {
                if (room.zone.y + room.zone.height <= pos.y && room.zone.y + room.zone.height >= pos.y - threshold) {
                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(room.zone.x, room.zone.y + room.zone.height + 15);
                    ctx.lineTo(pos.x, room.zone.y + room.zone.height + 15);
                    ctx.stroke();
                    ctx.closePath();
                    addText(room.zone.x, room.zone.y + room.zone.height + 50, (pos.x - room.zone.x), 1, ((pos.x - room.zone.x) / 100) + currentUnit, "right");

                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(pos.x, room.zone.y + room.zone.height);
                    ctx.lineTo(pos.x, room.zone.y + room.zone.height + 15);
                    ctx.stroke();
                    ctx.closePath();

                    ctx.beginPath();
                    ctx.strokeStyle = "#ffffff";
                    ctx.moveTo(room.zone.x + room.zone.width, room.zone.y + room.zone.height + 15);
                    ctx.lineTo(pos.x, room.zone.y + room.zone.height + 15);
                    ctx.stroke();
                    ctx.closePath();
                    addText(pos.x, room.zone.y + room.zone.height + 50, ((room.zone.x + room.zone.width) - pos.x), 1, ((room.zone.x + room.zone.width) - pos.x) + currentUnit, "right");
                }
                cursorPositionY = room.zone.y + room.zone.height;
            }
        });
    }

    if (!probeMode) {
        ctx.fillStyle = "#ffffff";
        ctx.arc(cursorPositionX, cursorPositionY, 2, 0, 2 * Math.PI);
        ctx.fill();
    } else {
        var width = 5;
        var height = 15;
        var positionX = cursorPositionX - (width / 2);
        var positionY = cursorPositionY - (height / 2);

        var room = getRoom(probeRoom);
        if (cursorPositionX > room.zone.x + room.zone.width) {
            positionX = (room.zone.x + room.zone.width) - width;
        }
        if (cursorPositionX < room.zone.x) {
            positionX = room.zone.x;
        }
        if (cursorPositionY > room.zone.y + room.zone.height) {
            positionY = (room.zone.y + room.zone.height) - height;
        }
        if (cursorPositionY < room.zone.y) {
            positionY = room.zone.y;
        }

        probePositionX = positionX;
        probePositionY = positionY;
        ctx.fillStyle = "#18b249";
        ctx.fillRect(positionX, positionY, width, height);

        if (showESPCoverage) {
            ctx.beginPath();
            ctx.fillStyle = "#ffffff40";
            ctx.arc(probePositionX, probePositionY, 500, 0, 2 * Math.PI);
            ctx.fill();
            ctx.closePath();
        }

        drawCross(positionX, positionY, room, height, width);
        drawCrossMesures(positionX, positionY, room, height, width);
    }

    // if we're not dragging, just return
    if (!isDown) {
        render(null, false);
        return;
    }

    // get the current mouse position
    mouseX = cursorPositionX; //parseInt(e.clientX - offsetX);
    mouseY = cursorPositionY; //parseInt(e.clientY - offsetY);

    // Put your mousemove stuff here



    // calculate the rectangle width/height based
    // on starting vs current mouse position
    var width = mouseX - startX;
    var height = mouseY - startY;

    // clear the canvas
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // draw a new rect from the start position 
    // to the current mouse position
    ctx.strokeRect(startX, startY, width, height);
    ctx.font = "15px Helvetica";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillStyle = "#ffffff";
    ctx.fillText('width : ' + Math.abs(width / 100) + currentUnit, (startX + (width / 2)), (startY + (height / 2)) - 10);
    ctx.fillText('height : ' + Math.abs(height / 100) + currentUnit, (startX + (width / 2)), (startY + (height / 2)) + 10);

    prevStartX = startX;
    prevStartY = startY;

    prevWidth = width;
    prevHeight = height;
}

function toggleProbeRadiuses() {
    showESPCoverage = !showESPCoverage;
    var tool = document.querySelector(".esp-coverage");
    if (showESPCoverage) {
        tool.classList.add("active");
    } else {
        tool.classList.remove("active");
    }
    render(null, false);
}

function drawCrossMesures(positionX, positionY, room, height, width) {
    ctx.font = "15px Helvetica";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillStyle = "#ffffff";

    //right wall
    ctx.fillText(Math.abs(((room.zone.x + room.zone.width) - positionX) / 100) + currentUnit, positionX + (((room.zone.x + room.zone.width) - positionX) / 2), positionY - 10);
    //left wall
    ctx.fillText(Math.abs((positionX - room.zone.x) / 100) + currentUnit, positionX - (positionX - room.zone.x) / 2, positionY - 10);
    //top wall
    ctx.fillText(Math.abs((positionY - room.zone.y) / 100) + currentUnit, positionX - 25, room.zone.y + ((positionY - room.zone.y) / 2));
    //bottom wall
    ctx.fillText(Math.abs((positionY - (room.zone.y + room.zone.height)) / 100) + currentUnit, positionX + 30, positionY + (((room.zone.y + room.zone.height) - positionY) / 2));
}

function drawCross(positionX, positionY, room, height, width) {

    // right wall
    ctx.beginPath();
    ctx.moveTo(positionX, positionY + (height / 2));
    ctx.lineTo(room.zone.x + room.zone.width, positionY + (height / 2));
    ctx.stroke();

    // left wall
    ctx.beginPath();
    ctx.moveTo(positionX, positionY + (height / 2));
    ctx.lineTo(room.zone.x, positionY + (height / 2));
    ctx.stroke();

    // top wall
    ctx.beginPath();
    ctx.moveTo(positionX + (width / 2), positionY + (height / 2));
    ctx.lineTo(positionX + (width / 2), room.zone.y);
    ctx.stroke();

    // bottom wall
    ctx.beginPath();
    ctx.moveTo(positionX + (width / 2), positionY + (height / 2));
    ctx.lineTo(positionX + (width / 2), room.zone.y + room.zone.height);
    ctx.stroke();
}

function getRoom(id) {
    var data = getRooms();
    return data.rooms.find(x => x.id == id);
}

// listen for mouse events
document.querySelector("#canvas").addEventListener("mousedown", function(e) {
    handleMouseDown(e);
});
document.querySelector("#canvas").addEventListener("mousemove", function(e) {
    handleMouseMove(e);
});
document.querySelector("#canvas").addEventListener("mouseup", function(e) {
    handleMouseUp(e);
});
document.querySelector("#canvas").addEventListener("mouseout", function(e) {
    handleMouseOut(e);
});

// document.querySelector("#canvas").onscroll = function(e) {
//     handleScroll(e);
// }

document.querySelector("#canvas").addEventListener("wheel", function(e) {
    handleScroll(e);
})


var mqtt;
var reconnectTimeout = 1200;

function onConnect() {
    console.log("connected to socket mqtt");
    mqtt.subscribe(mqttInitData.topic + "/#");
    document.querySelector(".hide-show-devices").disabled = false;
    toggleDevices();
}

function onFailure(msg) {
    console.log("failure to connect to mqtt");
    document.querySelector(".hide-show-devices").disabled = true;
    setTimeout(MQTTconnect, reconnectTimeout);
}

function onMessageArrived(msg) {
    var objectMsg = JSON.parse(msg.payloadString);
    objectMsg.x = (objectMsg.x  / 100);
    objectMsg.y = (objectMsg.y  / 100);
    upsertDevice(objectMsg);
    render(null, false);
    updateDevicesArray();
}

function toggleUnit() {
    var btn = document.querySelector(".unit-btn");
    var label = btn.querySelector(".txt-icn");
    if (currentUnit == "m") {
        currentUnit = "ft";
    } else {
        currentUnit = "m";
    }
    label.innerHTML = currentUnit;
    window.localStorage.setItem("unit", currentUnit);
    document.querySelector(".rooms").innerHTML = "";
    render();
}

function upsertDevice(objectMsg, color = null, hidden = null) {
    if (objectMsg) {
        var devicesString = window.localStorage.getItem("devices");

        var localDevices = {};

        if (devicesString) {
            localDevices = JSON.parse(devicesString);
        }
        //console.log("1", localDevices[objectMsg.name]);
        if (localDevices[objectMsg.name]) {
            localDevices[objectMsg.name].data = objectMsg;
            if (hidden !== null) {
                localDevices[objectMsg.name].hidden = hidden;
            }
            if (color !== null) {
                localDevices[objectMsg.name].color = color;
            }
        } else {
            console.log("creating new device");
            localDevices[objectMsg.name] = {
                data: objectMsg,
                color: color ? color : "#ffffff80",
                hidden: hidden ? hidden : false,
                id: uuidv4(),
            }
        }
        //console.log("2", localDevices[objectMsg.name]);
        devices = localDevices;
        window.localStorage.setItem("devices", JSON.stringify(localDevices));
    }
}

function MQTTconnect() {
    if (mqtt) {
        try{ mqtt.disconnect(); } catch {}
    }
    console.log("connecting to " + mqttInitData.host + " on port " + mqttInitData.port);
    mqtt = new Paho.MQTT.Client(mqttInitData.host, mqttInitData.port, "clientjs");
    var options = {
        timeout: 3,
        onSuccess: onConnect,
        userName: mqttInitData.username,
        password: mqttInitData.password,
        onFailure: onFailure,
        useSSL: mqttInitData.SSL,
    };
    mqtt.onMessageArrived = onMessageArrived;
    mqtt.connect(options);
}

var jsonStorage = getRooms();
if (jsonStorage.rooms.length) {
    var mostLeftRoom = jsonStorage.rooms.reduce(function(prev, curr) {
        return prev.zone.x <= curr.zone.x ? prev : curr;
    });

    // get highest y value
    var mostTopRoom = jsonStorage.rooms.reduce(function(prev, curr) {
        return prev.zone.y <= curr.zone.y ? prev : curr;
    });

    // create an offseter
    firstCoordinateOffsetY = mostTopRoom.zone.y;
    firstCoordinateOffsetX = mostLeftRoom.zone.x;
}

window.onload = function() {
    mqttInitData = getMqttSettings();
    if (mqttInitData.host && mqttInitData.port && mqttInitData.topic) {
        MQTTconnect();
    } else {
        document.querySelector(".hide-show-devices").disabled = true;
    }
}

function connectMQTT() {
    mqttInitData.host = document.querySelector(".mqtt-settings .host").value;
    mqttInitData.port = parseInt(document.querySelector(".mqtt-settings .port").value);
    mqttInitData.topic = document.querySelector(".mqtt-settings .topic").value;
    mqttInitData.username = document.querySelector(".mqtt-settings .username").value;
    mqttInitData.password = document.querySelector(".mqtt-settings .password").value;
    mqttInitData.SSL = document.querySelector(".mqtt-settings .SSL").checked;

    // not checking password to allow anonymous connections
    if (mqttInitData.host && mqttInitData.port && mqttInitData.topic) {
        MQTTconnect();
    }
}

function getMqttSettings() {
    var stringSettings = window.localStorage.getItem("mqttSettings");
    if (!stringSettings) {
        return mqttInitData;
    }

    var settings = JSON.parse(stringSettings);
    if (settings.SSL === undefined) {
        settings.SSL = false;
    }
    return settings;
}

function devicesSettings() {
    mqttInitData = getMqttSettings();

    var modal = document.querySelector(".mqtt-settings");
    modal.querySelector(".host").value = mqttInitData.host;
    modal.querySelector(".port").value = mqttInitData.port;
    modal.querySelector(".topic").value = mqttInitData.topic;
    modal.querySelector(".username").value = mqttInitData.username;
    modal.querySelector(".password").value = mqttInitData.password;
    modal.querySelector(".SSL").value = mqttInitData.SSL;
    //modal.querySelector(".coverage-color").value = probe.color.slice(0, -2);

    updateDevicesArray();

    modal.classList.add("visible");
}

function updateDevicesArray() {
    var body = document.querySelector(".mqtt-settings tbody");

    if (body) {
        var devicesString = window.localStorage.getItem("devices");
        var devices = JSON.parse(devicesString);
        if (devices) {
            var trs = [...body.querySelectorAll("tr")];
            var existingElementsId = trs.length ? trs.map(t => t.getAttribute("data-deviceId")) : [];
            
            var existingElements = Object.values(devices).filter(x => existingElementsId.includes(x.id));
            var nonExistingElements = Object.values(devices).filter(x => !existingElementsId.includes(x.id));

            existingElements.forEach(device => {
                var deviceName = device.data.name;
                var tr = body.querySelector("[data-deviceId='"+ device.id +"']");
                var name = tr.children[0];
                var color = tr.children[1];
                var x = tr.children[2];
                var y = tr.children[3];
                var z = tr.children[4];
                var hidden = tr.children[5];
                name.innerHTML = deviceName;
                x.innerHTML = Math.round(device.data.x * 10) / 10;
                y.innerHTML = Math.round(device.data.y * 10) / 10;
                z.innerHTML = Math.round(device.data.z * 10) / 10;
                if (color.value + "80" != device.color) {
                    color.value = device.color
                }
                hidden.checked = device.hidden;
            });

            nonExistingElements.forEach(device => {
                var deviceName = device.data.name;
                var tr = document.createElement("tr");
                tr.setAttribute("data-deviceId", device.id);
                var tdName = document.createElement("td");
                tdName.innerHTML = deviceName;
                var tdColor = document.createElement("td");
                var input = document.createElement("input");
                input.type = "color";
                input.value = device.color.slice(0, -2);
                input.setAttribute("data-device", device.id);
                input.addEventListener("change", (e) => {
                    var elem = e.target;
                    var deviceObj = getDeviceById(elem.getAttribute("data-device"));
                    upsertDevice(deviceObj.data, elem.value+"80");
                    render(null, false);
                    updateDevicesArray();
                });
                tdColor.append(input);
                var tdX = document.createElement("td");
                tdX.innerHTML = Math.round(device.data.x * 10) / 10;
                var tdY = document.createElement("td");
                tdY.innerHTML = Math.round(device.data.y * 10) / 10;
                var tdZ = document.createElement("td");
                tdZ.innerHTML = Math.round(device.data.z * 10) / 10;
                var tdHide = document.createElement("td");
                var checkbox = document.createElement("input");
                checkbox.classList.add("deviceCheckbox");
                checkbox.setAttribute("data-device", device.id);
                checkbox.type = "checkbox";
                checkbox.checked = device.hidden;
                checkbox.addEventListener("change", (e) => {
                    var elem = e.target;
                    var deviceObj = getDeviceById(elem.getAttribute("data-device"));
                    upsertDevice(deviceObj.data, null, elem.checked);
                    render(null, false);
                    updateDevicesArray();
                });
                tdHide.append(checkbox);

                tr.append(tdName);
                tr.append(tdColor);
                tr.append(tdX);
                tr.append(tdY);
                tr.append(tdZ);
                tr.append(tdHide);

                body.append(tr);
            })
        }
    }
}

function getDeviceById(id) {
    var key = Object.keys(devices).find(x => {
        return devices[x].id == id;
    });

    return devices[key];
}

function saveMqtt() {
    window.localStorage.setItem("mqttSettings", JSON.stringify(mqttInitData));
}

function toggleDevices() {
    if (showDevices) {
        showDevices = false;
        document.querySelector(".hide-show-devices").classList.remove("active");
    } else {
        showDevices = true;
        document.querySelector(".hide-show-devices").classList.add("active");
    }
    render(null, false);
}

document.querySelector(".yaml-data").addEventListener("keyup", event => {
    // Messed up and lost my localstorage, had to convert yaml back to json object and redraw.
    // it kinda junky, it works for yaml with only - rooms: and - floorplans: and bases itself on strict rules... like spaces and \n.
    // might not work for everyone from scratch.
    // This feature is disabled by default.
    var value = event.target.value;
    var regexMatch = /rooms:([\s\S]*?)(](?=[^\]]*$))/g.exec(value);
    var roomsString = regexMatch[0];
    var floorplanString = value.replace(roomsString, "");
    //var floorplanRooms = /(-([\s\S]*?)((?:[^\n]+\n){5}))/g.exec(floorplanString);
    var floorplanRooms = floorplanString.match(/(-([\s\S]*?)((?:[^\n]+\n){5}))/g);
    var idx = floorplanString.lastIndexOf('-')
    var lastRoom = floorplanString.substring(idx + 1).trim()
    floorplanRooms.push("- " + lastRoom);

    var storageDataConvert = {
        rooms: []
    }

    //console.log(floorplanRooms);
    floorplanRooms.forEach((room, roomIndex) => {
        var data = room.split("\n");
        var roomName = "";
        var roomId = -1;
        data.forEach((attr, index) => {
            if (attr.includes("- name:")) {
                roomName = attr.replace("- name:", "").trim();
                roomId = roomIndex + 1;
                roomX = 0;
                roomY = 0;
                storageDataConvert.rooms.push({
                    name: roomName,
                    id: roomId,
                    zone: {},
                    text: {
                        width: {
                            label: "width: n/a &nbsp;",
                        },
                        height: {
                            label: "height: n/a",
                        }
                    },
                    probes: []
                });
            } else if (attr.includes("y1:")) {
                var tmproom = storageDataConvert.rooms.find(x => x.id == roomId);
                if (tmproom) {
                    if (!tmproom.zone) {
                        tmproom.zone = {};
                    }
                    tmproom.zone.y = parseFloat(attr.replace("y1:", "").trim())  / 100;
                }
            } else if (attr.includes("x1:")) {
                var tmproom = storageDataConvert.rooms.find(x => x.id == roomId);
                if (tmproom) {
                    if (!tmproom.zone) {
                        tmproom.zone = {};
                    }
                    tmproom.zone.x = parseFloat(attr.replace("x1:", "").trim())  / 100;
                }
            } else if (attr.includes("y2:")) {
                var tmproom = storageDataConvert.rooms.find(x => x.id == roomId);
                var yValue = data.find(x => x.includes("y1:"))
                if (tmproom) {
                    if (!tmproom.zone) {
                        tmproom.zone = {};
                    }
                    tmproom.zone.height = (parseFloat(attr.replace("y2:", "").trim()) - parseFloat(yValue.replace("y1:", "").trim()))  / 100;
                }
            } else if (attr.includes("x2:")) {
                var tmproom = storageDataConvert.rooms.find(x => x.id == roomId);
                var xValue = data.find(x => x.includes("x1:"))
                if (tmproom) {
                    if (!tmproom.zone) {
                        tmproom.zone = {};
                    }
                    tmproom.zone.width = (parseFloat(attr.replace("x2:", "").trim()) - parseFloat(xValue.replace("x1:", "").trim()))  / 100;
                }
            }
        });
    })

    var probesString = roomsString.replace("rooms:", "").split("\n");
    probesString.forEach(probeString => {
        if (probeString) {
            var elems = probeString.split(":");
            var name = elems[0].replace(":", "").trim();
            var value = JSON.parse(elems[1].trim());
            var tmproom = storageDataConvert.rooms.find(x => x.name == name);
            if (tmproom) {
                tmproom.probes.push({
                    id: uuidv4(),
                    color: "#ffffff40",
                    coverage: 500,
                    x: value[0]  / 100,
                    y: value[1]  / 100,
                    z: value[2]  / 100,
                    width: 5,
                    height: 15
                });
            }
        }
    });
    convertedYAMLJSON = storageDataConvert;
    document.querySelector(".json-data").value = JSON.stringify(convertedYAMLJSON);
    //console.log(storageDataConvert);
});

function saveConvertedData() {
    window.localStorage.setItem("rooms", JSON.stringify(convertedYAMLJSON));
    window.location.reload();
}
