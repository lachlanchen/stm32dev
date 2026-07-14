# W25Q64 QSPI NOR Flash 只读探测记录

## 结论

STM32H743II 核心板上的 W25Q64 已确认实际装配并正常通信。探测过程只发送读取命令，没有发送 Write Enable、Page Program、Sector/Chip Erase 或状态寄存器写命令。

| 项目 | 实测结果 |
| --- | --- |
| JEDEC ID | `EF 40 17`，Winbond W25Q64，8 MiB |
| 重复读取 | `64/64` 完全一致 |
| SFDP | 签名 `SFDP` 有效 |
| 状态寄存器 | SR1=`00`、SR2=`02`、SR3=`60` |
| Quad Enable | SR2 bit1 已为 1；诊断程序没有修改它 |
| Unique ID | 8 字节可稳定读取；具体值不写入公开文档 |
| 地址 0 样本 | `FF FF FF FF`，仅说明这 4 字节处于擦除态 |
| 辅助读取错误 | `0` |
| 最终结果 | `PASS` |

## 硬件连接

W25Q64 焊在核心板上，通过 STM32 QUADSPI Bank 1 连接：

| STM32H743 引脚 | 复用功能 | W25Q64 信号 | 芯片引脚 |
| --- | --- | --- | ---: |
| `PB6` | AF10 QUADSPI | QCS / CS# | 1 |
| `PF9` | AF10 QUADSPI | QMISO / IO1 | 2 |
| `PF7` | AF9 QUADSPI | QWP / IO2 | 3 |
| GND | - | GND | 4 |
| `PF8` | AF10 QUADSPI | QMOSI / IO0 | 5 |
| `PB2` | AF9 QUADSPI | QSCK / CLK | 6 |
| `PF6` | AF9 QUADSPI | QHOLD / IO3 | 7 |
| 3.3 V | - | VCC | 8 |

## 独立诊断固件

源文件：`firmware/stm32_sensor_head_lcd/src/qspi_diag_main.c`。

诊断采用 Mode 3、20 MHz 保守时钟，只执行：

- `0x9F`：读取 JEDEC ID；
- `0x05/0x35/0x15`：读取三个状态寄存器；
- `0x5A`：读取 SFDP；
- `0x4B`：读取 Unique ID；
- `0x03`：读取地址 0 的四字节样本。

构建和烧录：

```powershell
make -C firmware/stm32_sensor_head_lcd qspi-diag
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg `
  -c "adapter speed 1000" `
  -c "program firmware/stm32_sensor_head_lcd/build_qspi_diag/stm32_qspi_diag.elf verify reset exit"
```

程序发布 `qspi_diag_*` 全局变量，便于 OpenOCD 在没有串口或 LCD GUI 时检查。测试结束后烧回长期使用镜像；本次已经恢复中文 NAND 诊断 GUI。

## 在未来固件中的定位

W25Q64 是可随机读取和可内存映射的 NOR Flash，容量小于 NAND，但更适合：

- 固件资源、字体、GUI 图片和查找表；
- 校准参数、配置和少量可靠日志；
- 第二固件镜像或 Bootloader 升级包；
- 配置 Memory-Mapped QSPI 后从 `0x90000000` 读取资源，甚至执行只读代码。

建议先制定固定分区表，再开放任何写入。未来驱动必须保留 SR2 的 QE 状态、检查 Busy/WEL、验证写后内容，并避免与可能的 Bootloader 资源区冲突。

## 与 NAND、SD 卡的分工

| 介质 | 推荐用途 |
| --- | --- |
| 内部 2 MiB Flash | 主 Bootloader 和关键固件 |
| W25Q64 8 MiB QSPI NOR | 快速随机读取资源、配置、备用镜像 |
| K9F2G08U0C 256 MiB SLC NAND | 大量实验数据、模型、日志；需要 ECC/坏块管理 |
| 32 GB SD 卡 | 可移动数据集、CSV、图像和跨电脑交换文件 |

32 GB SD 卡已经购买，但尚未到货和安装。未来代码不得假定 SD 卡在线；待用户确认插卡后，应先只读获取 CID/CSD/OCR、容量与文件系统，再决定是否写入。

