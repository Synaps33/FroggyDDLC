local textinput = "Player"
local keycursorX = 10
local keycursorY = 4
local caps = false
local keyboardrow1 = {'1','2','3','4','5','6','7','8','9','0'}
local keyboardrow2c = {'Q','W','E','R','T','Y','U','I','O','P'}
local keyboardrow3c = {'A','S','D','F','G','H','J','K','L','CAPS'}
local keyboardrow4c = {'Z','X','C','V','B','N','M','SPACE','DEL','ENTER'}
local keyboardrow2 = {'q','w','e','r','t','y','u','i','o','p'}
local keyboardrow3 = {'a','s','d','f','g','h','j','k','l','CAPS'}
local keyboardrow4 = {'z','x','c','v','b','n','m','SPACE','DEL','ENTER'}

local function rowFor(y)
	if y == 1 then return keyboardrow1 end
	if y == 2 then return caps and keyboardrow2c or keyboardrow2 end
	if y == 3 then return caps and keyboardrow3c or keyboardrow3 end
	return caps and keyboardrow4c or keyboardrow4
end

local function displayLabel(label)
	if label == 'CAPS' then return 'Aa' end
	if label == 'SPACE' then return 'SPC' end
	if label == 'ENTER' then return 'OK' end
	return label
end

function keyboard_draw()
	lg.push()
	lg.origin()

	lg.setColor(255,255,255,160)
	lg.rectangle('fill',0,0,320,240)
	lg.setColor(255,189,225,255)
	lg.rectangle('fill',18,16,284,208)
	lg.setColor(255,230,244,255)
	lg.rectangle('fill',22,20,276,200)

	lg.setFont(ui_riffic)
	lg.setColor(120,0,70,255)
	lg.print('PLAYER NAME',32,27)

	lg.setColor(255,255,255,255)
	lg.rectangle('fill',32,48,256,26)
	lg.setColor(255,189,225,255)
	lg.rectangle('line',32,48,256,26)
	lg.setFont(ui_aller)
	lg.setColor(60,0,40,255)
	local shown = textinput
	while ui_aller:getWidth(shown) > 246 and #shown > 1 do
		shown = string.sub(shown,2)
	end
	lg.print(shown,38,53)

	local startX = 31
	local rowY = {84,112,140,168}
	for row = 1,4 do
		local keys = rowFor(row)
		for col = 1,10 do
			local x = startX + (col-1)*26
			local y = rowY[row]
			local selected = (keycursorX == col and keycursorY == row)
			if selected then
				lg.setColor(255,120,185,255)
			else
				lg.setColor(255,238,248,255)
			end
			lg.rectangle('fill',x,y,24,24)
			if selected then
				lg.setColor(255,255,255,255)
			else
				lg.setColor(255,189,225,255)
			end
			lg.rectangle('line',x,y,24,24)

			local label = displayLabel(keys[col])
			local labelFont = ui_aller
			if label == 'Aa' or label == 'SPC' or label == 'DEL' or label == 'OK' then
				labelFont = ui_aller12
			end
			lg.setFont(labelFont)
			if selected then
				lg.setColor(255,255,255,255)
			else
				lg.setColor(60,0,40,255)
			end
			local tw = labelFont:getWidth(label)
			local th = labelFont:getHeight()
			lg.print(label,x+(24-tw)/2,y+(24-th)/2)
		end
	end

	lg.setFont(ui_aller12)
	lg.setColor(120,0,70,255)
	lg.print('A: type   B: delete   Y: caps'..(caps and ' [ON]' or ''),32,200)
	lg.pop()
end

function keyboard_keypressed(key)
	if key == 'left' and keycursorX > 1 then
		keycursorX = keycursorX - 1
	elseif key == 'right' and keycursorX < 10 then
		keycursorX = keycursorX + 1
	elseif key == 'up' and keycursorY > 1 then
		keycursorY = keycursorY - 1
	elseif key == 'down' and keycursorY < 4 then
		keycursorY = keycursorY + 1
	elseif key == 'a' then
		if keycursorY == 3 and keycursorX == 10 then
			caps = not caps
		elseif keycursorY == 4 and keycursorX == 8 then
			textinput = textinput..' '
		elseif keycursorY == 4 and keycursorX == 9 then
			textinput = string.sub(textinput,1,string.len(textinput)-1)
		elseif keycursorY == 4 and keycursorX == 10 then
			keyboard = false
			love.textinput(textinput)
		else
			local keys = rowFor(keycursorY)
			textinput = textinput..keys[keycursorX]
		end
	elseif key == 'b' then
		textinput = string.sub(textinput,1,string.len(textinput)-1)
	elseif key == 'y' then
		caps = not caps
	end
end
