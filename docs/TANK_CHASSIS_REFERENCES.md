# 十二款坦克的底盘与走行部参考

研究日期：2026-09-27。用于当前较写实的卡通模型：通过车首、发动机舱、轮组和
侧裙区别车型，保留[四级尺寸标准](TANK_PROPORTIONS.md)、已确认涂装和玩法挂点。
本文是建模依据与简化边界，不是十二辆精密复原模型已经完成的声明。

车型及主照片沿用[美军](US_TANK_REFERENCES.md)、[苏系](SOVIET_TANK_REFERENCES.md)、
[德国](GERMAN_TANK_REFERENCES.md)参考。车族简称不允许掩盖不同子型号零件混用。

## 走行部矩阵

数量均指**单侧负重轮的轴位**，不是双轮的轮片总数，不含驱动轮与诱导轮；
托带轮单独计数。前后以车体朝向为准。仅画一层可见轮面时，也应保留正确轴位节奏。

“直接”表示所列军方手册文字或馆藏说明直接支持；“同系”表示用明确注明的
同底盘车族资料辅助，并不把该来源冒充为选定子型号的完整原厂图纸。

| 车型 | 负重轮 / 托带轮 | 驱动端 | 必须表现的排列差异 | 证据 |
| --- | --- | --- | --- | --- |
| M4A3(75)，VVSS | 6 / 3 | 前 | 三组双轮台车，轮组间留缝；每组上方一托带轮；车尾诱导轮 | 直接：[S1](#s1--m4a3-vvss) |
| M26 | 6 / 5 | 后 | 独立大轮，五个上层托带轮；前诱导轮高于后驱动轮，不能沿用谢尔曼台车 | 直接：[S2](#s2--战时识别手册) |
| M60A3 | 6 / 3 | 后 | 六个独立双轮，三托带轮形成较疏上层；与 M26 的上层排列区别 | 直接：[S3](#s3--美国陆军识别教材) |
| M1A1 | 7 / 2 | 后 | 七个较小的双轮轴位；两个托带轮主要藏在侧裙内，前端诱导轮 | 同系 M1 维修手册 + M1A1 官方照片：[S4](#s4--abrams-走行部) |
| T-34/76，早期 L-11 | 5 / 0 | 后 | 五个近等距大轮，上段履带由大轮承托；小前诱导轮 | 直接 T-34 车族手册；早期型照片：[S2](#s2--战时识别手册)、[苏系 R1](SOVIET_TANK_REFERENCES.md#r1--t-3476早期小炮塔与斜车体) |
| IS-2，1944 年初 | 6 / 3 | 后 | 六个较小钢制轮位，上方三个小托带轮；不能复制 T-34 的五大轮 | 直接 IS-2 馆藏说明；早期车首另依选定藏车：[S5](#s5--is-2) |
| T-62，基本型 | 5 / 0 | 后 | 从车首向后数，前三轮较密，3–4、4–5 之间留明显大缝；不可照搬 T-55 的前部大缝 | 直接：[S6](#s6--t-62) |
| T-90A | 6 / 3 | 后 | 六个大轮，三个小托带轮，侧裙遮上部；仍要保留底部轮列的节奏 | 同系 T-90S 技术说明 + T-90A 馆藏照片：[S7](#s7--t-90a) |
| Panther A | 8 / 0 | 前 | 大轮交错、轮面前后分层，橡胶外缘；较小后诱导轮；不能八个小圆均匀摆一排 | 直接：[S2](#s2--战时识别手册)、[S8](#s8--panther-与-tiger-ii) |
| Tiger II，量产炮塔 | 9 / 0 | 前 | 九个轮臂轴位，五外四内的重叠钢轮；区别 Panther 更复杂的交错橡胶轮 | 历史说明译本：[S8](#s8--panther-与-tiger-ii) |
| Leopard 1，早期基本型 | 7 / 4 | 后 | 七个独立双轮、四托带轮，外露轮列；避免现代长侧裙遮成一块 | 同系馆藏/军方说明 + 德军早期型照片：[S9](#s9--leopard-1) |
| Leopard 2A4 | 7 / 4 | 后 | 七个近等距双轮、四托带轮；前厚后薄的侧裙覆盖上部，露出下半轮列 | 直接 Leopard 2 教材 + A4 官方参考：[S3](#s3--美国陆军识别教材)、[S10](#s10--leopard-2a4) |

轮位数能核实，不代表游戏中的轴距、轮径与离地间隙已经是实车尺寸。T-62 的两个
后部大间距是明确识别点；其余缺少尺寸图的间距可以归一化，但不能凭一个通用参数
把十二辆车都变成同一副底盘。M26 的五个托带轮和 Abrams 的两个托带轮尤其容易被
统一模板错误替换。

## 可实施的车体区别

下表是根据已归档馆藏/官方参考提炼的**原创造型方案**，不是测绘数据。先改变能在
游戏大小看见的结构，再选择一至两个甲板/排气细节；没有完整后视图的零件不宣称
达到博物馆复原精度。各车仍放在现有同级包络内，不能靠整体变大取得细节。

| 车型 | 车首与车体截面 | 发动机舱、尾部和侧面处理 |
| --- | --- | --- |
| M4A3 | 较高焊接上车体；倾斜车首下面单独塑出圆弧传动罩，两个前舱口形成肩部 | 后甲板用宽矩形栅格与检修面分区；窄挡泥板，完整露出台车。不要换成 M4A1 的全铸造圆车体或 HVSS 宽履带悬挂 |
| M26 | 低而宽，斜车首与内收下唇相接；顶部向后缓降，保留长后甲板 | 独立轮臂、薄翼板、后部矩形检修/通风分区；避免沿用谢尔曼高箱形发动机舱 |
| M60A3 | 车首以较饱满的铸造曲面接斜面，肩部转折软于 M26；正中前驾驶员舱口 | 后发动机舱比前甲板高，尾部两片较大通风结构；长翼板下保持轮组可见。不要套 M1 侧裙 |
| M1A1 | 低平楔形首上装甲，驾驶员位置居中；较宽平直的车体肩部 | 后部宽涡轮排气格栅为大识别点；长分段侧裙与前缘厚度区分；不复用豹二的尾部布局 |
| T-34/76 | 大斜面车首、侧面内收；前驾驶员舱门作为一块清楚的凸起，底部削尖 | 发动机甲板与斜后板形成两段；尾部一对短排气管，细翼板，五大轮完整外露 |
| IS-2 早期 | 厚重肩部，前驾驶员位置形成早期型的台阶/凸面；不是 IS-3 尖鼻，也不直接使用后期 IS-2 单一平斜板 | 小钢轮与厚履带形成重型下盘；后甲板有分离的格栅/检修块；有限外置筒形附件，避免每辆苏系都挂同样油桶 |
| T-62 | 平直低矮上车体、较长首上斜面和窄翼板；与 T-34 的高斜侧壁区别 | 左后侧排气口、低后甲板；右翼板可用少量方形储物/油箱块。基本型不加 T-62M 全长侧裙或马蹄形附加装甲 |
| T-90A | 低矮车首，以少量成组反应装甲板表达表面结构；不变成高直立箱体 | 左后侧排气、长分段软侧裙，前侧装甲分组较厚；保留六轮下缘。不用 T-90M 的尾舱或 T-80 的尾部排气布局 |
| Panther A | 长首上大斜板，车体侧面向上收窄；前传动区与尖削下车首清楚分开 | 后甲板双圆风扇区配矩形栅格；尾部细长立式排气轮廓；薄翼板/薄侧护板，不能做成现代厚裙板 |
| Tiger II | 宽而厚的大斜面车首，直硬肩线与更宽履带；不要退回虎 I 的直立首板 | 后甲板两大风扇区和较厚尾部排气护罩；折板翼板与侧护板；九轮钢轮列和 Panther 的八轮橡胶列形成对照 |
| Leopard 1 | 低矮楔形首部，前右驾驶员位置；细长车身的侧肩平直 | 后部侧面通风/排气格栅和紧凑甲板风扇区；早期基本型保留可见七轮，不借用 A5 增装附件 |
| Leopard 2A4 | 较宽的折面车首，前右驾驶员位置；宽肩但不复制 Abrams 中置前舱 | 厚前侧裙、较薄后侧裙，后部甲板与两侧尾部通风分区；拒绝将整片尾部画成 Abrams 的单一大涡轮格栅 |

历史车辆的外侧轮盘深度与裙板遮挡会使“可见圆圈数”小于轴位数。Panther/Tiger II
可以简化内层不可见轮片，但要保留前后错层与不同轮缘材质。托带轮在近景或低角度
才有意义，不为它们抬高整副履带、扩大黑缝或遮挡主要负重轮。

## 来源、署名与使用范围

以下均为链接研究，**没有下载、提取或导入照片、扫描页、模型或贴图**。只读取页面
文字、可检索手册片段并结合仓库已有照片参考；部分 PDF 本次只能核对索引文字，
未逐页查看。扫描件的第三方托管站不是原作者，也不自动赋予扫描图片开放许可。
没有明确许可的材料不进入游戏资源包。新网格继续由项目代码生成，按项目现有
PolyForm Noncommercial 1.0.0 条款；这不改变任何外部资料的权利归属。

### S1 — M4A3 VVSS

- [美国陆军 M4A3 技术手册扫描](https://www.theshermantank.com/wp-content/uploads/2015/12/TM9-752-TANK-MEDIUM-M4A3-44.pdf)：U.S. War Department，1944；索引内页标为 **TM 9-759**，URL 文件名写 9-752，不用文件名代替书内编号。走行部段落描述六组双轮台车总数和每组一个托带轮。
- [Museum of the American G.I. 的选定 M4A3 藏车](https://americangimuseum.org/collections/restored-vehicles/m4a3-sherman-1942-1943/)：摄影者及图片开放许可未单列；只链接。

### S2 — 战时识别手册

- [U.S. War Department，FM 30-40 扫描镜像](https://www.scribd.com/doc/144711139/Fm-30-40-Recognition-Pictorial-Manual-on-Armored-Vehicles-1943)：查看 M26、T-34、Panther 条目。镜像标题为 1943，但内容包含后续补页，不把 M26 条目声称为 1943 年已经发布。原作者为美国战争部，Scribd 为托管镜像；未确认上传者对扫描件的独立许可。
- M26 条目同时支持低车体、内收车首、缓降后甲板；T-34 条目支持斜板车体与五个大轮；Panther 条目说明八个双轮轴位。只归纳结构，不复制原插图。

### S3 — 美国陆军识别教材

- [IN 0535 Edition C，Lesson 1](https://www.globalsecurity.org/military/library/policy/army/accp/in0535/ch1.htm)：原作者 U.S. Army，GlobalSecurity 为转载；M60A3、Leopard 2 的走行部段落。Leopard 2 段落还区分侧裙前部厚装甲与后部增强橡胶板。
- 此份教材的 Leopard 1 托带轮数字与下述馆藏和德军同系底盘说明不一致；本项目采用后者的四个，而不是无条件复制一张通用识别表。未使用该页与本任务无关的装备性能、用户国或现代型号信息。

### S4 — Abrams 走行部

- [TM 9-2350-255-20-1-2-1，M1 hull maintenance](https://www.military-references.com/wp-content/uploads/books/tanks/usa/m1_abrams/M1_Abrams_Hull_Troubleshhoting_Manual_TM_9-2350-255-20-1-2-1.pdf)：美国陆军原手册，Military References 托管；第 2-10 节/图 2-1，七轮与两个托带轮。该册是 M1 车族资料，不能冒充完整的 M1A1 子型外形手册。
- [DVIDS M1A1 侧后照片](https://www.dvidshub.net/image/4458198/strong-europe-tank-challenge)：Matthias Fruth / U.S. Army，2018-06-06，VIRIN `180606-A-EO786-0191`；官方记录标记 Public Domain。仍只链接，未分发图片。

### S5 — IS-2

- [胜利博物馆：ИС-2](https://www.victorymuseum.ru/external-navigation/bronetekhnika/38136/)：馆方文字直接列出每侧六负重轮、三托带轮和后驱动轮；作者/图片许可未单列，只链接。
- [American Heritage Museum：1944 年 2 月 IS-2](https://www.americanheritagemuseum.org/jml/is-2-joseph-stalin/)：用于锁定早期车体而非 IS-2M、IS-3。馆藏说明不等于车首被遮挡部分已经取得完整工程图；摄影者及开放许可未单列。

### S6 — T-62

- [美国陆军 IN 0534 Edition D，Lesson 1](https://www.globalsecurity.org/military/library/policy/army/accp/in0534/lsn1.htm)：原文明确第 3–4、4–5 轮位之间的较大间距；GlobalSecurity 为教材转载。不要从页面中另列的改型提取基本型附件。
- [T-62 Operator's Manual 扫描](https://www.military-references.com/wp-content/uploads/books/tanks/ussr/t-62/T-62_Medium_Tank_Operators_Manual.pdf)：支持五大轮布局；本次未核实扫描版本封面的作者和年份，保留此限制。
- [The Tank Museum T-62](https://tankmuseum.org/tank-nuts/tank-collection/t-62/)：选定基本型馆藏参考，照片作者/开放许可未单列；现存沙色不是本项目涂装依据。

### S7 — T-90A

- [T-90S 技术说明与使用手册镜像](https://ru.scribd.com/document/622913002/%D0%A2-90-%D0%A2%D0%9E-%D0%B8-%D0%98%D0%AD)：第 16.1、16.1.4 节列全车十二负重轮、六托带轮；这是 **T-90S** 的同系辅助证据，手册版本、署名与扫描许可本次未核实，不混用其出口型附件。
- [Patriot Park T-90A 藏车与相册](https://parkpatriot.ru/o-parke/tekhnika-parka/t-90a/)：精确型号参考；馆方未单列摄影者或开放许可，只链接。T-90A 是俄罗斯时期型号，依用户要求归入游戏苏系阵营。

### S8 — Panther 与 Tiger II

- [1945 年美国军械目录 Panther 条目转录](https://www.lonesentry.com/ordnance/2010/07/23/panther-tank/)：原作者 Office of the Chief of Ordnance；Lone Sentry 为转录站，支持八轮轴位与车体斜面。不把战时敌军识别材料的所有技术数字都当作精确工程规范。
- [Tiger II 操作说明英文译本镜像](https://electronicsandbooks.com/edt/manual/Publischer/G/German%20Miltary%20WW2/Tiger%20Tanks%20c20070602%20%5B130%5D.pdf)：九个轮臂、五外四内、前驱动；含历史说明翻译与照片，翻译者及扫描许可未核实，只用于结构核对。
- 精确外形锚点：[American Heritage Museum Panther A](https://www.americanheritagemuseum.org/jml/panther/)、[The Tank Museum 量产 Tiger II](https://tankmuseum.org/tank_collection/tiger-ii)。博物馆照片作者和开放许可未单列。不得把虎王预生产炮塔照片用于替换已选量产炮塔。

### S9 — Leopard 1

- [Army Museum of Western Australia：Leopard AS1 馆藏](https://collectionswa.net.au/items/be78161e-dd04-4a04-994d-02b079c29ecf)：馆藏文字给出七负重轮、四托带轮；**AS1 是同系辅助，不是德国早期型的附件主参考**。
- [Bundeswehr：Pionierpanzer Dachs](https://www.bundeswehr.de/de/ausruestung-technik-bundeswehr/landsysteme-bundeswehr/pionierpanzer-dachs)：Leopard 1 底盘衍生车，军方说明再次列七轮、四托带轮。只核对基础走行部，工程车的车体、铲刀、吊臂不属于模型参考。
- [Bundeswehr Leopard 型号演变与 1967 年照片](https://www.bundeswehr.de/de/ausruestung-technik-bundeswehr/kampfpanzer-leopard-versionen-bundeswehr)：文章 Ole Henckel；早期 Leopard 1 照片 Bundeswehr / Günther Oed。未核实开放图像许可；只链接。

### S10 — Leopard 2A4

- [KNDS：LEOPARD 2 A4](https://knds.com/de/produkte/systeme/leopard/leopard-2-a4)：制造商型号页，辅助核对 A4 外形，不使用 A5/A6 附加楔形炮塔。
- [Bundeswehr 型号与官方照片](https://www.bundeswehr.de/de/ausruestung-technik-bundeswehr/kampfpanzer-leopard-versionen-bundeswehr)：A4 照片署名 Bundeswehr / Detmar Modes；与 S3 的 Leopard 2 走行部文字交叉核对。两站未确认开放图像许可，仅链接。

## 实施与验收边界

- 轮组、车首和后甲板分别按车型构建；共享材质、基础网格工具和四级包络仍可复用。
  同级三国的底盘与炮塔大小约束继续执行，不用实车米数直接替换游戏比例。
- 军方资料中的履带/悬挂工作原理用于辨认可见结构，不引入真实悬挂物理、传动模拟、
  碰撞改动或额外随机数；既有运动与炮口挂点保持。
- 同相机灰模先检查车首、轮列、尾部；再看国家颜色、默认游戏尺寸、四方向、
  Pixel Style 和 P1/P2。近景能数轮子不代表默认视角已能区别，最终要以渲染为准。
- 目前资料仍不是十二车完整正交图；甲板格栅条数、隐藏的内层轮片、螺栓、履带齿数
  和各子型的精确轴距只做有依据的简化。本页不宣称逐项达到实车测绘精度。
- 本次资料工作只新增文档，没有新分发资产，因此不改变资源包或第三方许可清单。
  构建、视觉迭代和测试结果由实际实现记录，不能用文献核对代替运行验收。
