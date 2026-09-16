import fs from "node:fs/promises";
import { SpreadsheetFile, Workbook } from "@oai/artifact-tool";

const outDir = "outputs/experiment-log";
const outFile = `${outDir}/温湿度监测系统实验记录表.xlsx`;
const wb = Workbook.create();
const font = "Arial";
const navy = "#1F4E78", blue = "#D9EAF7", input = "#FFF2CC", pale = "#F3F6F8";

function base(sheet, title, endCol) {
  sheet.showGridLines = false;
  sheet.getRange(`A2:${endCol}2`).merge();
  sheet.getRange("A2").values = [[title]];
  sheet.getRange("A2").format = { font: { name: font, size: 15, bold: true, color: "#1F1F1F" } };
  sheet.getRange(`A3:${endCol}3`).format.borders = { bottom: { style: "thin", color: navy } };
}
function header(range) {
  range.format = { fill: navy, font: { name: font, size: 10, bold: true, color: "#FFFFFF" },
    horizontalAlignment: "center", verticalAlignment: "center", wrapText: true,
    borders: { preset: "inside", style: "thin", color: "#FFFFFF" } };
}
function body(range) {
  range.format.font = { name: font, size: 10, color: "#222222" };
  range.format.verticalAlignment = "center";
  range.format.borders = { preset: "inside", style: "thin", color: "#D9E1E8" };
}

const guide = wb.worksheets.add("使用说明");
base(guide, "温湿度监测系统实验记录表", "H");
guide.getRange("A5:B13").values = [
  ["项目", "填写说明"],
  ["实验目的", "用少量、结构完整的数据验证测量误差、空间差异和控制安全性。"],
  ["精度与稳定性", "普通室内、较温暖、较潮湿三种工况，各记录10组；设备与参考仪器并排放置。"],
  ["三点布点", "入口、中心冠层、远端/角落各记录5组；移动测量时等待读数稳定。"],
  ["功能安全", "按测试用例逐项操作，填写实际结果、是否通过和证据。"],
  ["参考仪器", "记录型号、量程和标称精度；没有可靠参考仪器时，不把对比结果表述为绝对准确度。"],
  ["缺失数据", "无法测量时留空并在备注写明原因，不用0代替。"],
  ["单位", "温度统一使用 ℃，相对湿度统一使用 %RH。"],
  ["答辩使用", "优先展示 MAE、最大绝对误差、三点平均差异和功能测试通过情况。"],
];
header(guide.getRange("A5:B5")); body(guide.getRange("A6:B13"));
guide.getRange("A5:A13").format.font = { name: font, size: 10, bold: true };
guide.getRange("A5:A13").format.columnWidth = 18; guide.getRange("B5:B13").format.columnWidth = 72;
guide.getRange("B6:B13").format.wrapText = true; guide.getRange("A5:B13").format.autofitRows();
guide.tabColor = navy;

const acc = wb.worksheets.add("精度与稳定性");
base(acc, "精度与稳定性实验（3种工况 × 10组）", "L");
acc.getRange("A4:L4").values = [["黄色单元格为现场填写项；误差 = 设备值 − 参考值。空白数据不会参与统计。",null,null,null,null,null,null,null,null,null,null,null]];
acc.getRange("A4:L4").merge(); acc.getRange("A4").format.font = { name: font, size: 10, italic: true, color: "#666666" };
acc.getRange("A6:L6").values = [["序号","日期时间","工况","设备温度 (℃)","参考温度 (℃)","温度误差 (℃)","温度绝对误差 (℃)","设备湿度 (%RH)","参考湿度 (%RH)","湿度误差 (%RH)","湿度绝对误差 (%RH)","备注"]];
header(acc.getRange("A6:L6"));
const rows=[]; for(let i=1;i<=30;i++) rows.push([i,null,i<=10?"普通室内":i<=20?"较温暖":"较潮湿",null,null,null,null,null,null,null,null,null]);
acc.getRange("A7:L36").values=rows; body(acc.getRange("A7:L36"));
acc.getRange("B7:E36").format.fill=input; acc.getRange("H7:I36").format.fill=input; acc.getRange("L7:L36").format.fill=input;
acc.getRange("F7").formulas=[["=IF(OR(D7=\"\",E7=\"\"),\"\",D7-E7)"]]; acc.getRange("F7:F36").fillDown();
acc.getRange("G7").formulas=[["=IF(F7=\"\",\"\",ABS(F7))"]]; acc.getRange("G7:G36").fillDown();
acc.getRange("J7").formulas=[["=IF(OR(H7=\"\",I7=\"\"),\"\",H7-I7)"]]; acc.getRange("J7:J36").fillDown();
acc.getRange("K7").formulas=[["=IF(J7=\"\",\"\",ABS(J7))"]]; acc.getRange("K7:K36").fillDown();
acc.getRange("B7:B36").format.numberFormat="yyyy-mm-dd hh:mm"; acc.getRange("D7:K36").format.numberFormat="0.0";
acc.getRange("N2:O9").values=[["汇总指标","自动结果"],["有效数据组数",null],["设备温度平均值 (℃)",null],["温度 MAE (℃)",null],["温度最大绝对误差 (℃)",null],["设备湿度平均值 (%RH)",null],["湿度 MAE (%RH)",null],["湿度最大绝对误差 (%RH)",null]];
header(acc.getRange("N2:O2")); body(acc.getRange("N3:O9")); acc.getRange("N3:N9").format.fill=pale;
acc.getRange("O3:O9").formulas=[["=COUNT(D7:D36)"],["=IFERROR(AVERAGE(D7:D36),\"\")"],["=IFERROR(AVERAGE(G7:G36),\"\")"],["=IF(COUNT(G7:G36)=0,\"\",MAX(G7:G36))"],["=IFERROR(AVERAGE(H7:H36),\"\")"],["=IFERROR(AVERAGE(K7:K36),\"\")"],["=IF(COUNT(K7:K36)=0,\"\",MAX(K7:K36))"]];
acc.getRange("O4:O9").format.numberFormat="0.00";
acc.freezePanes.freezeRows(6); acc.getRange("A6:L36").format.rowHeight=24;
[["A:A",8],["B:B",20],["C:C",14],["D:K",17],["L:L",26],["N:N",27],["O:O",15]].forEach(([r,w])=>acc.getRange(r).format.columnWidth=w);
acc.tabColor="#5B9BD5";

const place = wb.worksheets.add("三点布点");
base(place,"简化布点实验（3个位置 × 5组）","G");
place.getRange("A4:G4").values=[["移动传感器后等待读数稳定；保持测试时段和其他条件尽量一致。",null,null,null,null,null,null]]; place.getRange("A4:G4").merge();
place.getRange("A4").format.font={name:font,size:10,italic:true,color:"#666666"};
place.getRange("A6:G6").values=[["序号","日期时间","位置","温度 (℃)","湿度 (%RH)","稳定等待时间 (min)","环境/备注"]]; header(place.getRange("A6:G6"));
const prow=[]; for(let i=1;i<=15;i++) prow.push([i,null,i<=5?"入口附近":i<=10?"中心冠层":"远端/角落",null,null,null,null]);
place.getRange("A7:G21").values=prow; body(place.getRange("A7:G21")); place.getRange("B7:G21").format.fill=input;
place.getRange("B7:B21").format.numberFormat="yyyy-mm-dd hh:mm"; place.getRange("D7:F21").format.numberFormat="0.0";
place.getRange("I2:K6").values=[["位置汇总","平均温度 (℃)","平均湿度 (%RH)"],["入口附近",null,null],["中心冠层",null,null],["远端/角落",null,null],["三点最大差值",null,null]]; header(place.getRange("I2:K2")); body(place.getRange("I3:K6"));
place.getRange("J3:K5").formulas=[["=IFERROR(AVERAGEIF(C7:C21,I3,D7:D21),\"\")","=IFERROR(AVERAGEIF(C7:C21,I3,E7:E21),\"\")"],["=IFERROR(AVERAGEIF(C7:C21,I4,D7:D21),\"\")","=IFERROR(AVERAGEIF(C7:C21,I4,E7:E21),\"\")"],["=IFERROR(AVERAGEIF(C7:C21,I5,D7:D21),\"\")","=IFERROR(AVERAGEIF(C7:C21,I5,E7:E21),\"\")"]];
place.getRange("J6:K6").formulas=[["=IF(COUNT(J3:J5)<3,\"\",MAX(J3:J5)-MIN(J3:J5))","=IF(COUNT(K3:K5)<3,\"\",MAX(K3:K5)-MIN(K3:K5))"]]; place.getRange("J3:K6").format.numberFormat="0.00";
place.freezePanes.freezeRows(6); [["A:A",8],["B:B",20],["C:C",16],["D:F",18],["G:G",32],["I:I",18],["J:K",19]].forEach(([r,w])=>place.getRange(r).format.columnWidth=w); place.tabColor="#70AD47";

const func = wb.worksheets.add("功能安全测试");
base(func,"报警、联动与安全功能测试","I");
func.getRange("A5:I5").values=[["编号","测试场景","操作/输入","预期结果","实际结果","是否通过","日期时间","测试人","证据/备注"]]; header(func.getRange("A5:I5"));
func.getRange("A6:I12").values=[
  [1,"正常范围","温湿度低于预警阈值","LED、蜂鸣器、自动风扇关闭",null,null,null,null,null],
  [2,"预警状态","温度≥28℃或湿度≥70%RH","LED闪烁、蜂鸣器间歇、风扇开启",null,null,null,null,null],
  [3,"严重报警","温度≥30℃或湿度≥80%RH","LED常亮、蜂鸣器持续、风扇开启",null,null,null,null,null],
  [4,"回差验证","报警后在阈值附近小幅波动","状态不频繁切换；降至退出阈值后恢复",null,null,null,null,null],
  [5,"安全覆盖","严重报警时发送 FAN OFF","拒绝危险关闭，风扇保持运行",null,null,null,null,null],
  [6,"报警静音","报警时发送 ALARM OFF 或 OK","蜂鸣器静音；LED、显示、风扇不受影响",null,null,null,null,null],
  [7,"传感器失效","断开 DHT22 DATA 或制造无效读数","显示读取失败、蜂鸣器停止、执行安全风扇策略",null,null,null,null,null],
];
body(func.getRange("A6:I12")); func.getRange("E6:I12").format.fill=input; func.getRange("F6:F12").dataValidation={rule:{type:"list",values:["通过","不通过","未测试"]}}; func.getRange("G6:G12").format.numberFormat="yyyy-mm-dd hh:mm";
func.getRange("A5:I12").format.wrapText=true; func.getRange("A6:I12").format.rowHeight=42; func.freezePanes.freezeRows(5);
[["A:A",8],["B:B",16],["C:C",29],["D:D",39],["E:E",29],["F:F",12],["G:G",20],["H:H",12],["I:I",28]].forEach(([r,w])=>func.getRange(r).format.columnWidth=w); func.tabColor="#ED7D31";

wb.recalculate();
await fs.mkdir(outDir,{recursive:true});
for (const [name,range,file] of [["使用说明","A1:H14","guide.png"],["精度与稳定性","A1:O18","accuracy.png"],["三点布点","A1:K21","placement.png"],["功能安全测试","A1:I12","function.png"]]) {
  const img=await wb.render({sheetName:name,range,scale:1,format:"png"});
  await fs.writeFile(`${outDir}/${file}`,new Uint8Array(await img.arrayBuffer()));
}
const check=await wb.inspect({kind:"table",range:"精度与稳定性!N2:O9",include:"values,formulas",tableMaxRows:10,tableMaxCols:3});
console.log(check.ndjson);
const errors=await wb.inspect({kind:"match",searchTerm:"#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A|#NUM!|#NULL!|#SPILL!|#CALC!",options:{useRegex:true,maxResults:100},summary:"final formula error scan"});
console.log(errors.ndjson);
const xlsx=await SpreadsheetFile.exportXlsx(wb); await xlsx.save(outFile); console.log(outFile);
