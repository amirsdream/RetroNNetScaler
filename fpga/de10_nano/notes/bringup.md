# DE10-Nano bring-up notes (fill in when board arrives)

## Clocks (typical GHRD)

- HPS clocks from dedicated PLLs  
- FPGA fabric video pixel clock: derive for chosen HDMI mode (e.g. 1024×768 or 1280×720)  
- TinySR pipeline clock: start 50–100 MHz; raise after timing closure  

## HDMI

- Chip: ADV7513 (Terasic reference design)  
- First milestone: solid color bars from FPGA → HDMI  
- Second: AXI/Avalon frame from HPS → scaler → HDMI  

## HPS → FPGA frame path (planned)

Options (pick one at bring-up):

1. **Lightweight HPS write** into on-chip dual-port RAM / FIFO (256×240×2 RGB565 ≈ 120 KiB — fits M10K with care)  
2. **DDR frame buffer** via FPGA SDRAM controller (more flexible, more complexity)  

For TinySR streaming, prefer: HPS writes a frame once → FPGA reads scanlines through a line-buffer CNN.

## Test vectors from PC

Capture from `4kscaler` later:

- `frame_in_256x240.rgb`  
- `frame_out_tinysr_1024x960.rgb`  

Use as RTL simulation expected output after int8 TinySR lands.
