"""Real retail exhibition constructor, separate native duration override."""
import pytest
from test_match_shortcuts_native import run, successful, word, EXE, ROM

pytestmark=pytest.mark.skipif(not EXE.exists() or not ROM.exists(),reason='requires build and user ROM')

@pytest.mark.parametrize('minutes',[1,20])
def test_native_duration_override_reaches_actual_match(tmp_path,minutes):
    result,ram=run(tmp_path,3200,settings=f'match_minutes={minutes}\n')
    successful(result)
    assert word(ram,0x70)==8
    assert word(ram,0xde07)==0
    raw=word(ram,0x16d0)
    seconds=((raw>>12)*10+((raw>>8)&15))*60+((raw>>4)&15)*10+(raw&15)
    assert max(0,minutes*60-90)<=seconds<=minutes*60, (minutes,hex(raw),seconds)
    assert word(ram,0x1e5a)<=2, 'native override escaped into cartridge enum'
