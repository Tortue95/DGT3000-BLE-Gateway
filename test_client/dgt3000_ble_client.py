#!/usr/bin/env python3
"""
DGT3000 BLE Gateway Test Client

Copyright (C) 2025 Tortue

Copyright (C) 2025 Tortue - d*g*t*3*0*0*0*(at)*t*e*d*n*e*t*.*f*r (remove all "*" to contact me)
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
"""

import asyncio
import json
import time
import uuid
from asyncio import Event
from datetime import datetime
from typing import Dict, Optional

import shlex

from bleak import BleakClient, BleakScanner
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.box import ROUNDED
from rich.text import Text
from prompt_toolkit import PromptSession
from prompt_toolkit.history import FileHistory
from prompt_toolkit.patch_stdout import patch_stdout

# DGT3000 BLE Service UUIDs
DGT3000_SERVICE_UUID       = "73822f6e-edcd-44bb-974b-93ee97cb0000"
PROTOCOL_VERSION_CHAR_UUID = "73822f6e-edcd-44bb-974b-93ee97cb0001"
COMMAND_CHAR_UUID          = "73822f6e-edcd-44bb-974b-93ee97cb0002"
EVENT_CHAR_UUID            = "73822f6e-edcd-44bb-974b-93ee97cb0003"
STATUS_CHAR_UUID           = "73822f6e-edcd-44bb-974b-93ee97cb0004"

# Device name to look for
DEVICE_NAME = "DGT3000-Gateway"

# Global console for rich output
console = Console(force_terminal=True)

class DGT3000BLEClient:
    """BLE client for DGT3000 Gateway communication."""
    
    def __init__(self):
        self.client: Optional[BleakClient] = None
        self.device_address: Optional[str] = None
        self.connected = False
        self.dgt_configured_event = Event()
        self.command_responses: Dict[str, Event] = {}
        self.response_data: Dict[str, Dict] = {}
        self.stats = {
            'commands_sent': 0,
            'responses_received': 0,
            'events_received': 0,
            'connection_time': None
        }
    
    async def scan_devices(self, timeout: float = 10.0) -> list:
        """Scan for DGT3000 Gateway devices."""
        console.print(f"[blue]Scanning for BLE devices (timeout: {timeout}s)...[/blue]")
        
        devices = await BleakScanner.discover(timeout=timeout)
        dgt_devices = []
        
        for device in devices:
            if device.name and DEVICE_NAME in device.name:
                dgt_devices.append(device)
                console.print(f"[green]Found DGT3000 Gateway: {device.name} ({device.address})[/green]")
        
        if not dgt_devices:
            console.print("[yellow]No DGT3000 Gateway devices found[/yellow]")
        
        return dgt_devices
    
    async def connect(self, address: Optional[str] = None) -> bool:
        """Connect to DGT3000 Gateway device."""
        if not address:
            devices = await self.scan_devices()
            if not devices:
                console.print("[red]No devices found to connect to[/red]")
                return False
            address = devices[0].address
        
        console.print(f"[blue]Connecting to {address}...[/blue]")
        
        try:
            self.client = BleakClient(str(address))
            await self.client.connect()
            
            self.device_address = address
            self.connected = True
            self.stats['connection_time'] = datetime.now()
            
            # Get and verify protocol version FIRST
            console.print("[blue]Check protocol version...[/blue]")
            try:
                protocol_version = await self.get_protocol_version()
                if protocol_version == "1.0":
                    console.print(f"[green]✅ Protocol version (v{protocol_version}) is 1.0. Client is compatible.[/green]")
                else:
                    console.print(f"[red]❌ Protocol version (v{protocol_version}) is not 1.0. Client might be incompatible.[/red]")
            except Exception as e:
                console.print(f"[red]Error verifying protocol version: {e}[/red]")

            # Subscribe to event notifications
            await self.client.start_notify(EVENT_CHAR_UUID, self._event_notification_handler)
            
            # Wait for the DGT3000 to be configured by listening for the connectionStatus event
            console.print("[blue]Waiting for DGT3000 to be configured...[/blue]")
            try:
                await asyncio.wait_for(self.dgt_configured_event.wait(), timeout=5.0)
                console.print("[green]✅ DGT3000 is configured (event received).[/green]")
                
                # Show text and beep on DGT 3000
                await self.display_text(" Connected", 2)
                console.print(f"[green]Connected to DGT3000 Gateway at {address}[/green]")

            except asyncio.TimeoutError:
                console.print("[red]❌ Timed out waiting for DGT3000 configuration event. Aborting connection.[/red]")
                return False

            return True # Only return True if all steps (BLE, protocol, DGT config) are successful
            
        except Exception as e:
            console.print(f"[red]Connection failed: {e}[/red]")
            return False # Return False on any connection exception
    
    async def disconnect(self):
        """Disconnect from device."""
        if self.client and self.connected:
            try:
                await self.client.stop_notify(EVENT_CHAR_UUID)
                await self.client.disconnect()
                console.print("[yellow]Disconnected from DGT3000 Gateway[/yellow]")
            except Exception as e:
                console.print(f"[red]Disconnect error: {e}[/red]")
        
        self.connected = False
        self.client = None
        self.device_address = None
    
    def _event_notification_handler(self, sender, data: bytearray):
        """Handle incoming event notifications."""
        try:
            event_json = data.decode('utf-8')
            console.print(f"[blue]📥 Received event JSON:[/blue]\n[yellow]{event_json}[/yellow]")
            event = json.loads(event_json)
            
            self.stats['events_received'] += 1
            
            event_type = event.get('type', 'unknown')
            event_data = event.get('data', {})
            
            if event_type == 'timeUpdate':
                left_time = f"{event_data.get('leftHours', 0):02d}:{event_data.get('leftMinutes', 0):02d}:{event_data.get('leftSeconds', 0):02d}"
                right_time = f"{event_data.get('rightHours', 0):02d}:{event_data.get('rightMinutes', 0):02d}:{event_data.get('rightSeconds', 0):02d}"
                
                console.print(f"[cyan]⏱️ Time Update: {left_time} | {right_time}[/cyan]")
                
            elif event_type == 'buttonEvent':
                button = event_data.get('button', 'unknown')
                is_repeat = event_data.get('isRepeat', False)
                repeat_count = event_data.get('repeatCount', 0)
                
                repeat_info = f" (repeat #{repeat_count})" if is_repeat else ""
                console.print(f"[yellow]🔘 Button: {button}{repeat_info}[/yellow]")
                
            elif event_type == 'connectionStatus':
                connected = event_data.get('connected', False)
                configured = event_data.get('configured', False)
                status_icon = "🟢" if connected and configured else "🟡" if connected else "🔴"
                console.print(f"[blue]{status_icon} DGT Connection Status: {'Connected & Configured' if connected and configured else 'Connected' if connected else 'Disconnected'}[/blue]")
                
                # If we receive the event that the DGT is configured, set the asyncio event
                if configured:
                    self.dgt_configured_event.set()
                
            elif event_type == 'error':
                error_code = event_data.get('errorCode', 0)
                error_message = event_data.get('errorMessage', 'Unknown error')
                console.print(f"[red]❌ Error {error_code}: {error_message}[/red]")

            elif event_type == 'command_response':
                response_id = event.get('id')
                if response_id in self.command_responses:
                    self.response_data[response_id] = event
                    self.command_responses[response_id].set()

                # Also log it for debugging
                status = event.get('status', 'N/A')
                icon = "✅" if status == "success" else "❌"
                result_data = event.get('result', event.get('data', {}))
                console.print(f"[magenta]{icon} Response for command {response_id} (Status: {status}) -> {json.dumps(result_data)}[/magenta]")
            else:
                console.print(f"[cyan]📡 Event: {event_type} -> {json.dumps(event.get('data', {}))}[/cyan]")
            
        except Exception as e:
            console.print(f"[red]Error processing event: {e}[/red]")
    
    async def send_command(self, command: str, params: Optional[Dict] = None, command_id: Optional[str] = None) -> Dict:
        """Send a command to the DGT3000 Gateway."""
        if not self.connected or not self.client:
            raise Exception("Not connected to device")
        
        if not command_id:
            command_id = str(uuid.uuid4())[:8]
        
        cmd_data = {
            "id": command_id,
            "command": command,
            "timestamp": int(time.time() * 1000)
        }
        
        if params:
            cmd_data["params"] = params
        
        cmd_json = json.dumps(cmd_data)
        
        try:
            # Create an event to wait for the response
            response_event = Event()
            self.command_responses[command_id] = response_event

            console.print(f"[blue]📤 Sending command JSON:[/blue]\n[yellow]{cmd_json}[/yellow]") # Print the JSON
            await self.client.write_gatt_char(COMMAND_CHAR_UUID, cmd_json.encode('utf-8'))
            self.stats['commands_sent'] += 1
            
            console.print(f"[blue]📤 Command sent: {command} (ID: {command_id})[/blue]")
            
            # Wait for response
            response = await self._wait_for_response(command_id, timeout=5.0)
            return response
            
        except Exception as e:
            console.print(f"[red]Error sending command: {e}[/red]")
            raise
    
    async def _wait_for_response(self, command_id: str, timeout: float = 5.0) -> Dict:
        """Wait for a command response using an asyncio event."""
        response_event = self.command_responses.get(command_id)
        if not response_event:
            raise Exception(f"No response event found for command {command_id}")

        try:
            await asyncio.wait_for(response_event.wait(), timeout=timeout)
            
            # Get response data and clean up
            response = self.response_data.pop(command_id, None)
            self.command_responses.pop(command_id, None)
            
            if response:
                self.stats['responses_received'] += 1
                return response
            else:
                # This should not happen if the event was set correctly
                raise Exception("Response event was set, but no data was found.")

        except asyncio.TimeoutError:
            # Clean up on timeout
            self.command_responses.pop(command_id, None)
            raise TimeoutError(f"No response received for command {command_id} within {timeout}s")
    
    async def get_status(self) -> Dict:
        """Get device status."""
        if not self.connected or not self.client:
            raise Exception("Not connected to device")
        
        try:
            status_data = await self.client.read_gatt_char(STATUS_CHAR_UUID)
            status_json = status_data.decode('utf-8')
            status = json.loads(status_json)
            
            console.print("[green]📊 Status retrieved successfully[/green]")
            return status
            
        except Exception as e:
            console.print(f"[red]Error getting status: {e}[/red]")
            raise
    
    async def get_protocol_version(self) -> str:
        """Get the BLE protocol version."""
        if not self.connected or not self.client:
            raise Exception("Not connected to device")
        
        try:
            version_data = await self.client.read_gatt_char(PROTOCOL_VERSION_CHAR_UUID)
            version = version_data.decode('utf-8')
            console.print(f"[green]Protocol Version retrieved: \"{version}\"[/green]")
            return version
        except Exception as e:
            console.print(f"[red]Error getting protocol version: {e}[/red]")
            raise
    
    async def set_time(self, left_mode: int, left_hours: int, left_minutes: int, left_seconds: int,
                      right_mode: int, right_hours: int, right_minutes: int, right_seconds: int) -> Dict:
        """Set timer values."""
        params = {
            "leftMode": left_mode,
            "leftHours": left_hours,
            "leftMinutes": left_minutes,
            "leftSeconds": left_seconds,
            "rightMode": right_mode,
            "rightHours": right_hours,
            "rightMinutes": right_minutes,
            "rightSeconds": right_seconds
        }
        return await self.send_command("setTime", params)
    
    async def display_text(self, text: str, beep_duration: int = 0, left_dots: int = 0, right_dots: int = 0) -> Dict:
        """Display text on DGT3000."""
        params = {
            "text": text,
            "beep": beep_duration, # Renamed parameter for clarity
            "leftDots": left_dots,
            "rightDots": right_dots
        }
        return await self.send_command("displayText", params)
    
    async def end_display(self) -> Dict:
        """End text display."""
        return await self.send_command("endDisplay")
    
    async def stop_timers(self) -> Dict:
        """Stop all timers."""
        return await self.send_command("stop")
    
    async def run_timers(self, left_mode: int, right_mode: int) -> Dict:
        """Start timers."""
        params = {
            "leftMode": left_mode,
            "rightMode": right_mode
        }
        return await self.send_command("run", params)
    
    async def get_time(self) -> Dict:
        """Get current timer values."""
        return await self.send_command("getTime")
    
    def print_stats(self):
        """Print connection statistics."""
        table = Table(title="DGT3000 BLE Client Statistics")
        table.add_column("Metric", style="cyan")
        table.add_column("Value", style="green")
        
        table.add_row("Commands Sent", str(self.stats['commands_sent']))
        table.add_row("Responses Received", str(self.stats['responses_received']))
        table.add_row("Events Received", str(self.stats['events_received']))
        
        if self.stats['connection_time']:
            uptime = datetime.now() - self.stats['connection_time']
            table.add_row("Connection Uptime", str(uptime).split('.')[0])
        
        table.add_row("Connected", "Yes" if self.connected else "No")
        table.add_row("Device Address", self.device_address or "None")
        
        console.print(table)
    
async def interactive(address: Optional[str] = None):
    """Interactive mode for testing commands."""
    client = DGT3000BLEClient()
    
    if not await client.connect(address):
        return
    
    # Setup command history
    HISTORY_FILE = '.dgt3000_client_history'
    session = PromptSession(history=FileHistory(HISTORY_FILE))

    console.print("[green]Connected! Type 'help' for available commands, 'quit' to exit.[/green]")
    
    try:
        with patch_stdout(raw=True):
            while True:
                try:
                    cmd_line = await session.prompt_async("DGT3000> ")
                    if not cmd_line.strip():
                        continue
                    
                    parts = shlex.split(cmd_line)
                    if not parts:
                        continue
                    
                    command = parts[0]
                    args = parts[1:]

                    if command == 'quit':
                        break
                    elif command == 'help':
                        print_help()
                    elif command == 'status':
                        status = await client.get_status()
                        console.print(Panel(json.dumps(status, indent=2), title="Device Status"))
                    elif command == 'stats':
                        client.print_stats()
                    elif command == 'display':
                        if not args:
                            console.print("[red]Usage: display <text> [beep] [left_dots] [right_dots][/red]")
                            continue
                        
                        text = args[0]
                        beep = int(args[1]) if len(args) > 1 else 0
                        left_dots = int(args[2]) if len(args) > 2 else 0
                        right_dots = int(args[3]) if len(args) > 3 else 0
                        await client.display_text(text, beep, left_dots, right_dots)

                    elif command == 'end_display':
                        await client.end_display()
                    elif command == 'stop':
                        await client.stop_timers()
                    elif command == 'set_time':
                        if len(args) == 8:
                            params = [int(p) for p in args]
                            await client.set_time(*params)
                        else:
                            console.print("[red]Usage: set_time <left_mode> <left_h> <left_m> <left_s> <right_mode> <right_h> <right_m> <right_s>[/red]")
                    elif command == 'run':
                        if len(args) == 2:
                            await client.run_timers(int(args[0]), int(args[1]))
                        else:
                            console.print("[red]Usage: run <left_mode> <right_mode>[/red]")
                    elif command == 'get_time':
                        response = await client.get_time()
                        if response and response.get('status') == 'success':
                            result = response.get('result', {})
                            left_time = f"{result.get('leftHours', 0):02d}:{result.get('leftMinutes', 0):02d}:{result.get('leftSeconds', 0):02d}"
                            right_time = f"{result.get('rightHours', 0):02d}:{result.get('rightMinutes', 0):02d}:{result.get('rightSeconds', 0):02d}"
                            console.print(f"Current Time: [bold green]{left_time}[/bold green] | [bold green]{right_time}[/bold green]")
                        else:
                            console.print(f"[red]Get time failed: {response}[/red]")
                    else:
                        console.print(f"[red]Unknown command: {command}[/red]")

                except ValueError:
                    console.print("[red]Invalid argument. Please check command usage.[/red]")
                except IndexError:
                    console.print("[red]Not enough arguments for command. Please check command usage.[/red]")
                    
    except Exception as e:
        console.print(f"[red]An unexpected error occurred: {e}[/red]")
    
    finally:
        await client.disconnect()

def print_help():
    """Print available commands using rich."""
    console.print(Panel.fit("[bold cyan]DGT3000 BLE Client Help[/bold cyan]", border_style="blue", padding=(0, 5)))

    # Timer Control
    timer_table = Table(show_header=False, box=None, padding=(0, 1))
    timer_table.add_column("Command", style="yellow", width=35)
    timer_table.add_column("Description")
    timer_table.add_row("set_time <l_m> <l_h> ... <r_s>", "Set timer values (8 int parameters)")
    timer_table.add_row("run <left_mode> <right_mode>", "Start timers with specified modes")
    timer_table.add_row("stop", "Stop all timers")
    timer_table.add_row("get_time", "Get current timer values")
    console.print(Panel(timer_table, title="[bold green]Timer Control[/bold green]", border_style="green", title_align="left"))

    # Display Control
    display_table = Table(show_header=False, box=None, padding=(0, 1))
    display_table.add_column("Command", style="yellow", width=35)
    display_table.add_column("Description")
    display_table.add_row("display <text> [beep] [ld] [rd]", "Display text with optional beep and dots")
    display_table.add_row("end_display", "End text display")
    console.print(Panel(display_table, title="[bold green]Display Control[/bold green]", border_style="green", title_align="left"))
    
    # System Commands
    system_table = Table(show_header=False, box=None, padding=(0, 1))
    system_table.add_column("Command", style="yellow", width=35)
    system_table.add_column("Description")
    system_table.add_row("status", "Get device status")
    system_table.add_row("stats", "Show connection statistics")
    system_table.add_row("help", "Show this help message")
    system_table.add_row("quit", "Exit interactive mode")
    console.print(Panel(system_table, title="[bold green]System Commands[/bold green]", border_style="green", title_align="left"))

    # Examples
    examples_text = Text.from_markup("""[yellow]Basic text display:[/yellow]
  display "Hello World"

[yellow]Text with beep:[/yellow]
  display "Beep!" 10             [dim]# Beep for 10 units[/dim]

[yellow]Text with dots:[/yellow]
  display "Check Dots" 0 1 6     [dim]# No beep, some flag on[/dim]

[yellow]Timer operations:[/yellow]
  set_time 1 1 30 0 1 1 30 0     [dim]# Set timer parameters[/dim]
  run 1 2                        [dim]# Start with modes 1 and 2[/dim]
  stop                           [dim]# Stop all timers[/dim]""")
    console.print(Panel(examples_text, title="[bold magenta]Examples[/bold magenta]", border_style="magenta", padding=(1, 2), title_align="left"))

if __name__ == '__main__':
    try:
        asyncio.run(interactive())
    except KeyboardInterrupt:
        console.print("[yellow]Exiting DGT3000 BLE Client gracefully.[/yellow]")
