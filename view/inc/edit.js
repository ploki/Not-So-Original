
var Draggable = Class.create();

Draggable.prototype = {
	
	onMouseDown: function(e) {
		this.xoff=e.clientX-this.toolbox.offsetLeft;
		this.yoff=e.clientY-this.toolbox.offsetTop;
		this.toolbox.style.cursor='-moz-grabbing';
		this.handle.style.cursor='-moz-grabbing';
		if ( e.ctrlKey ) {
			this.move=false;
			Event.observe(document, "mousemove", this.eventResizeElement);
		}
		else {
			this.move=true;
			Event.observe(document, "mousemove", this.eventMoveElement);
		}
		Event.observe(document, "mouseup", this.eventMouseUp);
		Event.stop(e);
	},

	onMouseUp: function(e) {
		this.toolbox.style.cursor='auto';
		this.handle.style.cursor='-moz-grab';
		if ( this.move )
			Event.stopObserving(document, "mousemove", this.eventMoveElement);
		else
			Event.stopObserving(document, "mousemove", this.eventResizeElement);
		Event.stopObserving(document, "mouseup", this.eventMouseUp);
		Event.stop(e);
	},

	moveElement:function(e) {
		this.toolbox.style.top=e.clientY-this.yoff+"px";
		this.toolbox.style.left=e.clientX-this.xoff+"px";
		Event.stop(e);
	},

	resizeElement: function(e) {

		this.toolbox.style.height=e.clientY - this.toolbox.offsetTop +"px";
		this.toolbox.style.width =e.clientX - this.toolbox.offsetLeft+"px";
//		alert(window.innerWidth);
		this.handle.style.width=this.toolbox.clientWidth-8+"px";
		Event.stop(e);
	},

	initialize: function(toolbox,handle) {
		this.toolbox=$(toolbox);
		this.handle=$(handle);
		this.oldmousemove = null;
		this.oldmouseup = null;
		this.xoff=0;
		this.yoff=0;
		this.move=true;

		this.handle.style.cursor='-moz-grab';
		this.toolbox.style.zIndex=1000;
		this.toolbox.style.overflow="scroll";
		this.handle.style.position="fixed";
		this.handle.style.width=this.toolbox.clientWidth-8+"px";
		this.toolbox.style.paddingTop="20px";
		this.handle.style.marginTop="-20px";
/*
		this.handle.style.zIndex=99;
		this.handle.style.marginTop="-20px";
		this.handle.style.marginBottom="-20px";
		this.handle.style.marginLeft="-20px";
		this.handle.style.marginRight="-20px";
//		this.handle.style.width="100px";
//		this.handle.style.height="100px";
//		this.handle.style.zIndex=100;
*/		
		this.eventMouseDown  = this.onMouseDown.bindAsEventListener(this);
		this.eventMouseUp    = this.onMouseUp.bindAsEventListener(this);
		this.eventMoveElement  = this.moveElement.bindAsEventListener(this);
		this.eventResizeElement  = this.resizeElement.bindAsEventListener(this);

		Event.observe(this.handle, "mousedown", this.eventMouseDown);


	}
};

/*
var tbh = document.getElementById('plop');
	var tb = document.getElementById('toolbox');
	var xoff;
	var yoff;
	tbh.onmousedown = function (e) {
		xoff=e.clientX-tb.offsetLeft;
		yoff=e.clientY-tb.offsetTop;
		function move(e) {
			tb.style.top=e.clientY-yoff+"px";
			tb.style.left=e.clientX-xoff +"px";
		}
		window.onmousemove = move;
		return false;
	};
	window.onmouseup = function(e) { 
		window.onmousemove = null;
		return false;
	}
*/
