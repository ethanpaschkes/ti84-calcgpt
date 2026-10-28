# Setting up the Raspberry Pi Zero 2 W bridge

## What to buy
| Item | Notes |
|---|---|
| **Raspberry Pi Zero 2 W**, board only | About $16 at CanaKit when checked on 2026-10-24 (prices vary, shop around). Not "Zero 2 WH" (headers) and not a kit; you do not need either. |
| **microSD card, 16 GB** | About $6-8. Any decent brand (SanDisk, Samsung). 8 GB works too. |
| **micro-USB OTG adapter** | A tiny adapter: micro-USB plug on one end, a normal USB-A *socket* on the other. About $1-5. |
| **Power for the Pi** | A phone charger (5 V, 2 A or more) or a power bank, plus a **micro-USB cable**. You probably have both. |
| **Your calculator's cable** | The USB-A to mini-USB cable you already use with TI Connect CE. |
| **microSD card reader for the Mac** | Only if the Mac has no card slot (a Mac mini usually doesn't). About $6-10. |

Total: about $25-30. The Pi has **two micro-USB sockets**: the one labelled **PWR IN** is for power only; the one labelled **USB**
is the data socket that goes to the calculator.

## Part 1: prepare the memory card (on the Mac)
1. Download **Raspberry Pi Imager** from raspberrypi.com/software and open it. Put the card in the Mac.
2. Device: **Raspberry Pi Zero 2 W**. Operating system: **Raspberry Pi OS (other) > Raspberry Pi OS Lite (64-bit)**. Storage: your card.
3. When it offers to customise settings, say yes and fill in:
   - Hostname: `calcbridge`
   - Username: `calc` (the service file expects this name) and a password you will remember
   - Wi-Fi: your home network's name and password. **The Zero 2 W only uses 2.4 GHz Wi-Fi**, so if your router has separate 2.4 and
     5 GHz names, use the 2.4 GHz one. Set your country.
   - Services: turn on **SSH** (password login)
4. Write the card. This erases that card (nothing else on the Mac).

## Part 2: first start
1. Put the card in the Pi. Plug the power cable into the socket labelled **PWR IN**.
2. Wait about 3 minutes for the first start. (The green light flickers, then settles.)
3. In the Terminal app on the Mac run `ssh calc@calcbridge.local` and answer `yes` to the question, then type the password.
   If `calcbridge.local` is not found, wait another minute and try again. If it never works, your router may be blocking devices from
   talking to each other (a "guest network" or "client isolation" setting).

## Part 3: put the bridge program on the Pi
Open a **second** Terminal window on the Mac (leave the Pi one alone) and run these, one at a time:

    python3 ~/Developer/claude/ti84/wireless/calcnet.py token     # only makes sure the secret exists; ignore what it prints, never share it
    scp ~/Developer/claude/ti84/wireless/bridge_linux.py ~/Developer/claude/ti84/wireless/calcbridge.service calc@calcbridge.local:~/
    scp ~/.calcnet-token calc@calcbridge.local:~/

(`scp` copies files to the Pi; it asks for the Pi's password each time.) The secret now exists on both computers, and nowhere else.

## Part 4: the first test, by hand
1. **Mac**, Terminal: `python3 ~/Developer/claude/ti84/wireless/calcnet.py listen` (leave it running). It prints that it is waiting.
2. **Calculator**: start a program that uses the link, such as **CALCGPT**. Plug the calculator in: Pi data socket (labelled **USB**) >
   OTG adapter > your cable > calculator.
3. **Pi**, in the first Terminal: `ls /dev/ttyACM*`. You should see `/dev/ttyACM0`. (If not, run `dmesg | tail` and tell me what it says.)
4. **Pi**: `python3 bridge_linux.py --mac Ethans-Mac-Mini.local`. It should print `connected`, and the Mac's `calcnet` window should say `bridge connected`.
5. **Mac**, a third Terminal: `CALC_PORT=~/.calc-port python3 ~/Developer/claude/ti84/calcgpt/mac/calcgptd.py`, then ask a question on the calculator.
   Stop the bridge any time with Ctrl+C.

## Part 5: start by itself every time the Pi has power
On the Pi:

    sudo cp ~/calcbridge.service /etc/systemd/system/
    sudo systemctl enable --now calcbridge
    systemctl status calcbridge      # should say "active (running)"

After this you only need: power on the Pi, plug in the calculator, run `calcnet.py listen` on the Mac, and use the Mac programs with `CALC_PORT=~/.calc-port`.
(The service file contains the Mac's name `Ethans-Mac-Mini.local`; if you rename the Mac or the Pi's user, edit it.)

## If something does not work
- **No `/dev/ttyACM0`**: the calculator program must already be running when you plug in (the port only exists then); check the OTG
  adapter is on the Pi's **USB** socket, not PWR IN; try another OTG adapter or cable.
- **"waiting for the Mac"**: the Mac's `calcnet.py listen` is not running, or the Pi cannot find `Ethans-Mac-Mini.local`. Use the Mac's
  address instead (`192.168.1.50` when checked; ask the router to keep it fixed): `--mac 192.168.1.50`.
- **Connection refused / secret not accepted**: the two `~/.calcnet-token` files differ. Copy the Mac's again with `scp`.
- **Slow or choppy**: 2.4 GHz Wi-Fi is shared and busy; move the Pi closer to the router.

## Safety
Anyone who knows the secret and is on your Wi-Fi can act as the calculator, and the data is not encrypted. Keep the secret private
and use only your home network.
