//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

const frames = [];
for (let i = 0; i < 80; i++) {
  let ang = i * Math.PI * 2 / 60;
  let dist = i;
  let x = Math.cos(ang) * i;
  let y = Math.sin(ang) * i / 2;
  frames.push({
    x: Math.round(x),
    y: Math.round(y)
  });
}

const cmds = [];
function addcmd(str, n) {
  while (n < -100) {
    cmds.push(str + '(-100),');
    n += 100;
  }
  while (n > 100) {
    cmds.push(str + '(100),');
    n -= 100;
  }
  if (n !== 0) cmds.push(str + '(' + n + '),');
}
const ADDX = (n) => addcmd('ADDX', n);
const ADDY = (n) => addcmd('ADDY', n);

let lastx = 0;
let lasty = 0;
for (let i = frames.length - 1; i >= 0; i--) {
  const f = frames[i];
  ADDX(f.x - lastx);
  ADDY(f.y - lasty);
  if (i === frames.length - 1) {
    cmds.push('WAIT(n*3),');
  } else {
    cmds.push('WAIT(1),');
  }
  lastx = f.x;
  lasty = f.y;
}
const jump = cmds.length;
cmds.push('WAIT(240),');
for (let dy = -2; dy <= 2; dy++) {
  ADDY(dy);
  ADDX(-Math.sign(dy));
  cmds.push('WAIT(1),');
  ADDY(dy);
  cmds.push('WAIT(1),');
}
cmds.push('WAIT(240),');
cmds.push('JUMP(' + (jump - cmds.length) + '),');
cmds[cmds.length - 1] = cmds[cmds.length - 1].replace(/,$/, '');
console.log(cmds.join(''));
