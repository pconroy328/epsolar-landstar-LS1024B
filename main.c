/* 
 * File:   main.c
 * Author: pconroy
 *
 * Created on June 6, 2018, 1:16 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <modbus/modbus.h>

#define LANDSTAR_1024B_ID       0x01

// 
// Forward declarations
static void     getRatedData( modbus_t *ctx );
static void     getRealTimeData( modbus_t *ctx );
static void     getRealTimeStatus( modbus_t *ctx );
static void     getSettings( modbus_t *ctx );
static void     getStatisicalParameters( modbus_t *ctx );
static char     *batteryTypeToString( uint16_t batteryType );


/*
 * Notes from the LS Series Modbus data sheet
    (1) The ID of the controller is 1 by default and can be modified by PC software (Solar Station Monitor ) or remote meter MT50.
    (2) The serial communication parameters: 115200bps baudrate, 8 data bits, 1 stop bit and no parity, no handshaking.
    (3) The register address below is in hexadecimal format.
    (4) For the data with the length of 32 bits, such as power, using the L and H registers represent the low and high 16 bits value,respectively. 
        e.g.The charging input rated power is actually 3000W, multiples of 100 times, then the value of 0x3002 register is 0x93E0 and value of 0x3003 is 0x0004
 */

int main (int argc, char* argv[]) 
{
    modbus_t    *ctx;
    
    puts( "Opening ttyUSB0, 115200 8N1" );
    ctx = modbus_new_rtu( "/dev/ttyUSB0", 115200, 'N', 8, 1 );
    if (ctx == NULL) {
        fprintf( stderr, "Unable to create the libmodbus context\n" );
        return -1;
    }

    printf( "Setting slave ID to %X\n", LANDSTAR_1024B_ID );
    modbus_set_slave( ctx, LANDSTAR_1024B_ID );

    puts( "Connecting" );
    if (modbus_connect( ctx ) == -1) {
        fprintf( stderr, "Connection failed: %s\n", modbus_strerror( errno ) );
        modbus_free( ctx );
        return -1;
    }
    
    getRatedData( ctx );
    getRealTimeData( ctx );
    getRealTimeStatus( ctx );
    getSettings( ctx );
    getStatisicalParameters( ctx );
    
    puts( "Done" );
    modbus_close( ctx );
    return (EXIT_SUCCESS);
}

// -----------------------------------------------------------------------------
static 
void    getRealTimeData (modbus_t *ctx)
{
    int         registerAddress = 0x3100;
    int         numBytes = 0x13;                  // 0x14 and up gives 'illegal data address' error
    uint16_t    buffer[ 32 ];
    
    memset( buffer, '\0', sizeof buffer );
    
    if (modbus_read_input_registers( ctx, registerAddress, numBytes, buffer ) == -1) {
        fprintf(stderr, "getRealTimeData() - Read failed: %s\n", modbus_strerror( errno ));
        return;
    }
    
    // ---------------------------------------------
    //  Photo Voltaic Values - Volts, Amps and Watts
    float pvArrayVoltage =  ((float) buffer[ 0x00 ]) / 100.0;
    float pvArrayCurrent =  ((float) buffer[ 0x01 ]) / 100.0;
    
    //
    // Assemble the Power (watts) value from two words
    long    temp = buffer[ 0x03 ] << 16;
    temp |= buffer[ 0x02 ];
    float pvArrayPower   =  (float) temp / 100.0;
    
    
    // ---------------------------------------------
    //  Battery Values - Volts, Amps and Watts
    float batteryVoltage =  ((float) buffer[ 0x04 ]) / 100.0;
    float batteryCurrent =  ((float) buffer[ 0x05 ]) / 100.0;

    temp = buffer[ 0x07 ] << 16;
    temp |= buffer[ 0x06 ];
    float batteryPower   =  (float) temp / 100.0;
    
    // ---------------------------------------------
    //  Load Values - Volts, Amps and Watts
    float loadVoltage =  ((float) buffer[ 0x0C ]) / 100.0;
    float loadCurrent =  ((float) buffer[ 0x0D ]) / 100.0;

    temp    = buffer[ 0x0F ] << 16;
    temp |= buffer[ 0x0E ];
    float loadPower   =  (float) temp / 100.0;
    
    
    float   batteryTemp =  ((float) buffer[ 0x10 ]) / 100.0;
    float   caseTemp =  ((float) buffer[ 0x11 ]) / 100.0;
    float   componentsTemp =  ((float) buffer[ 0x12 ]) / 100.0;
 
    //  Our LS1024B controller doesn't seem to support any register data above 0x12
    //float   batterySOC =  ((float) buffer[ 0x1A ]) / 100.0;
    //float   remoteBatteryTemp =  ((float) buffer[ 0x1B ]) / 100.0;
    //float   systemRatedVoltage =  ((float) buffer[ 0x1D ]) / 100.0;

    
    puts(   "-- Real Time Data from Controller --" );
    printf( "PV Array Voltage: %0.2f V\n", pvArrayVoltage );
    printf( "PV Array Current: %0.2f A\n", pvArrayCurrent );
    printf( "PV Array Power  : %0.2f W\n", pvArrayPower );

    printf( "Battery Voltage: %0.2f V\n", batteryVoltage );
    printf( "Battery Current: %0.2f A\n", batteryCurrent );
    printf( "Battery Power  : %0.2f W\n", batteryPower );

    printf( "Load Voltage: %0.2f V\n", loadVoltage );
    printf( "Load Current: %0.2f A\n", loadCurrent );
    printf( "Load Power  : %0.2f W\n", loadPower );
    
    printf( "Battery Temperature   : %0.1f *C\n", batteryTemp );
    printf( "Case Temperature      : %0.1f *C\n", caseTemp );
    printf( "Components Temperature: %0.1f *C\n", componentsTemp );
}


// -----------------------------------------------------------------------------
static
void    getRealTimeStatus (modbus_t *ctx)
{
    int         registerAddress = 0x3200;
    int         numBytes = 0x2;
    uint16_t    buffer[ 32 ];
    
    memset( buffer, '\0', sizeof buffer );
    
    if (modbus_read_input_registers( ctx, registerAddress, numBytes, buffer ) == -1) {
        fprintf(stderr, "getRealTimeStatus() - Read failed: %s\n", modbus_strerror( errno ));
        return;
    }
    
    uint16_t    batteryStatus =  buffer[ 0x00 ];
    /*
     *  D3-D0: 01H Overvolt , 00H Normal , 02H Under Volt, 03H Low Volt Disconnect, 04H Fault
     *  D7-D4: 00H Normal, 01H Over Temp.(Higher than the warning settings), 02H Low Temp.( Lower than the warning settings),
     *  D8: Battery inerternal resistance abnormal 1, normal 0
     *  D15: 1-Wrong identification for rated voltage
     */
    
    
    uint16_t    chargingStatus =  buffer[ 0x01 ];
    /*
     *  D15-D14: Input volt status. 00 normal, 01 no power connected, 02H Higher volt input, 03H Input volt error.
     *  D13: Charging MOSFET is short.
     *  D12: Charging or Anti-reverse MOSFET is short.
     *  D11: Anti-reverse MOSFET is short.
     *  D10: Input is over current.
     *  D9: The load is Over current.
     *  D8: The load is short.
     *  D7: Load MOSFET is short.
     *  D4: PV Input is short.
     *  D3-2: Charging status. 00 No charging, 01 Float, 02 Boost, 03 Equlization.
     *  D1: 0 Normal, 1 Fault
     */
    
    puts(   "-- Real Time Status from Controller --" );
    printf( "Battery Status : %0X \n", batteryStatus );
    printf( "Charging Status: %0X \n", chargingStatus );
}


// -----------------------------------------------------------------------------
static
void    getSettings (modbus_t *ctx)
{
    int         registerAddress = 0x9000;
    int         numBytes = 0x0A;                    // 0x10 and up gives 'illegal data address' error
    uint16_t    buffer[ 32 ];

    memset( buffer, '\0', sizeof buffer );
    
    
    /* int modbus_write_and_read_registers(modbus_t *ctx, int write_addr, int write_nb, const uint16_t *src, int read_addr, int read_nb, const uint16_t *dest); */
    if (modbus_read_registers( ctx, registerAddress, numBytes, buffer ) == -1) {
        fprintf(stderr, "getSettings() - Read failed: %s\n", modbus_strerror( errno ));
        return;
    }
    
    uint16_t    batteryType =  buffer[ 0x00 ];
    uint16_t    batteryCapacity =  buffer[ 0x01 ];
    
    float   tempCompensationCoeff   = ((float) buffer[ 0x02 ]) / 100.0;
    float   highVoltageDisconnect   = ((float) buffer[ 0x03 ]) / 100.0;
    float   chargingLimitVoltage    = ((float) buffer[ 0x04 ]) / 100.0;
    float   overVoltageReconnect    = ((float) buffer[ 0x05 ]) / 100.0;
    float   equalizationVoltage     = ((float) buffer[ 0x06 ]) / 100.0;
    float   boostVoltage            = ((float) buffer[ 0x07 ]) / 100.0;
    float   floatVoltage            = ((float) buffer[ 0x08 ]) / 100.0;
    float   boostReconnectVoltage   = ((float) buffer[ 0x09 ]) / 100.0;

    //  Our LS1024B controller doesn't seem to support any register data above 0x0A
    //float   lowVoltageReconnect     = ((float) buffer[ 0x0A ]) / 100.0;
    //float   underVoltageRecover     = ((float) buffer[ 0x0B ]) / 100.0;
    //float   underVoltageWarning     = ((float) buffer[ 0x0C ]) / 100.0;
    //float   lowVoltageDisconnect    = ((float) buffer[ 0x0D ]) / 100.0;
    //float   dischargingLimitVoltage = ((float) buffer[ 0x0E ]) / 100.0;
    //uint16_t    realTimeClock1      = buffer[ 0x13 ];
    //uint16_t    realTimeClock2      = buffer[ 0x14 ];
    //uint16_t    realTimeClock3      = buffer[ 0x15 ];
    //  There are more fields...
    
    puts(   "-- Settings from Controller --" );
    printf( "Battery Type: %s\n", batteryTypeToString( batteryType ) );
    printf( "Battery Rated Capacity: %d AH\n", batteryCapacity );
    printf( "High Voltage Disconnect: %0.2f V\n", highVoltageDisconnect );
    printf( "Charging Limit Voltage: %0.2f V\n", chargingLimitVoltage );
    printf( "Over Voltage Reconnect: %0.2f V\n", overVoltageReconnect );
    printf( "Equilization Voltage: %0.2f V\n", equalizationVoltage );
    printf( "Boost Voltage Disconnect: %0.2f V\n", boostVoltage );
    printf( "Float Voltage Disconnect: %0.2f V\n", floatVoltage );
    printf( "Boost Voltage Reconnect: %0.2f V\n", boostReconnectVoltage );
}

// -----------------------------------------------------------------------------
static 
void    getRatedData (modbus_t *ctx)
{
    int         registerAddress = 0x3000;
    int         numBytes = 0x09;                  // 0x0A and up gives 'illegal data address' error
    uint16_t    buffer[ 32 ];
    
    memset( buffer, '\0', sizeof buffer );
    
    if (modbus_read_input_registers( ctx, registerAddress, numBytes, buffer ) == -1) {
        fprintf(stderr, "getRatedData() - Read failed: %s\n", modbus_strerror( errno ));
        return;
    }

    
    float   pvArrayRatedVoltage     = ((float) buffer[ 0x00 ]) / 100.0;
    float   pvArrayRatedCurrent     = ((float) buffer[ 0x01 ]) / 100.0;

    long temp  = buffer[ 0x03 ] << 16;
    temp |= buffer[ 0x02 ];
    float pvArrayRatedPower =  (float) temp / 100.0;

    float   batteryRatedVoltage     = ((float) buffer[ 0x04 ]) / 100.0;
    float   batteryRatedCurrent     = ((float) buffer[ 0x05 ]) / 100.0;
 
    temp  = buffer[ 0x07 ] << 16;
    temp |= buffer[ 0x06 ];
    float batteryRatedPower =  (float) temp / 100.0;

    uint16_t    chargingMode = buffer[ 0x08 ];                  // 0x01 == PWM
    
    puts(   "-- Rated Data from Controller --" );
    printf( "PV Rated Voltage: %0.2f V\n", pvArrayRatedVoltage );
    printf( "PV Rated Current: %0.2f A\n", pvArrayRatedCurrent );
    printf( "PV Rated Power: %0.2f W\n", pvArrayRatedPower );
    printf( "Battery Rated Voltage: %0.2f V\n", batteryRatedVoltage );
    printf( "Battery Rated Current: %0.2f A\n", batteryRatedCurrent );
    printf( "Battery Rated Power: %0.2f W\n", batteryRatedPower );
    printf( "Charging Mode: %0X\n", chargingMode );
}

// -----------------------------------------------------------------------------
static 
void    getStatisicalParameters (modbus_t *ctx)
{
    int         registerAddress = 0x3300;
    int         numBytes = 0x1E;                  
    uint16_t    buffer[ 32 ];
    
    memset( buffer, '\0', sizeof buffer );
    
    if (modbus_read_input_registers( ctx, registerAddress, numBytes, buffer ) == -1) {
        fprintf(stderr, "getStatisicalParameters() - Read failed: %s\n", modbus_strerror( errno ));
        return;
    }

    float   maximumInputVoltageToday = ((float) buffer[ 0x00 ]) / 100.0;
    float   minimumInputVoltageToday = ((float) buffer[ 0x01 ]) / 100.0;
    float   maximumBatteryVoltageToday = ((float) buffer[ 0x02 ]) / 100.0;
    float   minimumBatteryVoltageToday = ((float) buffer[ 0x03 ]) / 100.0;

    long temp  = buffer[ 0x05 ] << 16;
    temp |= buffer[ 0x04 ];
    float consumedEnergyToday =   (float) temp / 100.0;

    temp  = buffer[ 0x07 ] << 16;
    temp |= buffer[ 0x06 ];
    float consumedEnergyMonth =   (float) temp / 100.0;

    temp  = buffer[ 0x09 ] << 16;
    temp |= buffer[ 0x08 ];
    float consumedEnergyYear =   (float) temp / 100.0;
    
    temp  = buffer[ 0x0B ] << 16;
    temp |= buffer[ 0x0A ];
    float totalConsumedEnergy =   (float) temp / 100.0;
    
    temp  = buffer[ 0x0D ] << 16;
    temp |= buffer[ 0x0C ];
    float generatedEnergyToday =   (float) temp / 100.0;
    
    temp  = buffer[ 0x0F ] << 16;
    temp |= buffer[ 0x0E ];
    float generatedEnergyMonth =   (float) temp / 100.0;

    temp  = buffer[ 0x11 ] << 16;
    temp |= buffer[ 0x10 ];
    float generatedEnergyYear =   (float) temp / 100.0;

    temp  = buffer[ 0x13 ] << 16;
    temp |= buffer[ 0x12 ];
    float totalGeneratedEnergy =   (float) temp / 100.0;
    
    temp  = buffer[ 0x15 ] << 16;
    temp |= buffer[ 0x14 ];
    float CO2Reduction =   (float) temp / 100.0;
    
    temp  = buffer[ 0x1C ] << 16;
    temp |= buffer[ 0x1B ];
    float batteryCurrent =   (float) temp / 100.0;
    
    float batteryTemp =   ((float) buffer[ 0x01D ]) / 100.0;
    float ambientTemp =   ((float) buffer[ 0x01E ]) / 100.0;
    
    puts(   "-- Statisical Parameters from Controller --" );
    printf( "Max PV Input Voltage Today: %0.2f V\n", maximumInputVoltageToday );
    printf( "Min PV Input Voltage Today: %0.2f V\n", minimumInputVoltageToday );
    printf( "Max Battery Voltage Today: %0.2f V\n", maximumBatteryVoltageToday );
    printf( "Min Battery Voltage Today: %0.2f V\n", minimumBatteryVoltageToday );
    
    printf( "Consumed Energy Today: %0.1f KWH\n", consumedEnergyToday );
    printf( "Consumed Energy Month: %0.1f KWH\n", consumedEnergyMonth );
    printf( "Consumed Energy Year: %0.1f KWH\n", consumedEnergyYear );
    printf( "Total Consumed Energy: %0.1f KWH\n", totalConsumedEnergy );
    
    printf( "Generated Energy Today: %0.1f KWH\n", generatedEnergyToday );
    printf( "Generated Energy Month: %0.1f KWH\n", generatedEnergyMonth );
    printf( "Generated Energy Year: %0.1f KWH\n", generatedEnergyYear );
    printf( "Total Generated Energy: %0.1f KWH\n", totalGeneratedEnergy );
    
    printf( "Carbon Dioxide Reduction %0.1f Ton\n", CO2Reduction );

    printf( "Battery Temp %0.1f *C\n", batteryTemp );
    printf( "Ambient Temp %0.1f *C\n", ambientTemp );

}

//------------------------------------------------------------------------------
static
char    *batteryTypeToString (uint16_t batteryType)
{
    switch (batteryType) {
        case    0x00:   return "User Defined";  break;
        case    0x01:   return "Sealed";        break;
        case    0x02:   return "Gel";           break;
        case    0x03:   return "Flooded";       break;
        
        default:        return "Unknown";
    }
}
