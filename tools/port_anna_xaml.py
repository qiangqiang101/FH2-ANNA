"""Ports FH3's ANNA control (media/UI.zip Controls/HUDAnnaControl.xaml) to FH2, into
assets/HUDAnnaControl.xaml, which FH2 ANNA loads instead of FH2's own (src/anna_xaml.cpp).

    python tools/port_anna_xaml.py "D:\\Games\\FH2\\Opus_1.0.125.0_x64__8wekyb3d8bbwe"

FH3's layout is kept as it is: the 300 px round minimap with its opacity-mask textures, the
inactive edge (two ed:Arcs, with a gap for the label), the edge glow (a TextureMaskEffect over
FH3's glow texture, faded by a gradient), the label, and the list, masked by a circle so it
starts at the minimap's edge. Only what FH2 doesn't have is changed:
  - FH3's textures come from FH2 ANNA's own folder (converted by tools/fh3_textures.py);
  - FH3's "A_FZMaterialPresetAdditive" is a plain preset there (FH2's of that name is additive),
    and its HUD item glass is FH2's A_FZMaterialPresetGlass_Standard; its HUD text is 26 pt;
  - the controller-button controls (FH3's ANNA input commands) are FH2's d-pad pictures, and the
    label is the d-pad and FH3's ANNA logo (no Kinect: no microphone, never "unavailable"); the
    label's and the highlighted row's d-pad pictures are resources of their own, FH2ANNA_Icon_Open
    and FH2ANNA_Icon_Choose, for a keyboard mod (FH2Mouse) to show its keys in;
  - FH3's IsHighlighted is FH2's IsSelected (FH2 ANNA highlights with it); ElementToTrack isn't
    FH2's; the minimap keeps FH2's "hud" map profile; the sounds are FH2's;
  - FH2's HUD puts the control at 54, 92 from the bottom left: moved so the minimap's edges are on
    the HUD's safe area, about 110 from the left, the ring's centre level with the speedometer's
    (measured in the game: 24 higher than the speedometer's 300 x 23 layout says);
  - the list's circular mask is on a Grid, not an FZMaterial, and the Decorators are Grids
    (FH2's FZMaterial and Decorator scale their content to fit).
"""
import os
import re
import sys
import zipfile

TEXTURES = ['ANNA_Glow_On', 'ANNA_Glow_ANNAOnly', 'ANNA_Logo', 'VerticalGradient', 'Minimap_OpacityMask',
            'Minimap_OpacityMask_WithDestination', 'Minimap_OpacityMask_MapOnly',
            'Minimap_OpacityMask_MapOnly_WithDestination']

RESOURCES = '''
            <!-- FH2 ANNA: what FH3's control uses that FH2 hasn't, or has under other names -->
            <local:FZMaterialPreset x:Key="FH2ANNA_Preset_Plain" />
            <!-- ANNA's buttons, its own pictures (of the pad's): a keyboard mod (FH2Mouse) finds them
                 here by name and shows its keys in them -->
            <ImageSource x:Key="FH2ANNA_Icon_Open">/ForzaUI;component/Textures/Data_Bound/ControllerIcons/Controller_DPadDown.png</ImageSource>
            <ImageSource x:Key="FH2ANNA_Icon_Choose">/ForzaUI;component/Textures/Data_Bound/ControllerIcons/Controller_DPadRight.png</ImageSource>
            <ImageSource x:Key="FH2ANNA_Icon_Cinematic">/ForzaUI;component/Textures/Data_Bound/ControllerIcons/Controller_LB.png</ImageSource>
            <Style x:Key="FH2ANNA_Text" TargetType="{x:Type local:FZTextBlock}" BasedOn="{StaticResource A_Style_FZTextBlock_HudMedium}">
                <Setter Property="FontSize" Value="26" />
            </Style>
            <Style x:Key="FH2ANNA_GlassHudItem" TargetType="local:FZMaterial">
                <Setter Property="Color" Value="{StaticResource A_Color_HUD_ItemBackground}" />
                <Setter Property="Preset" Value="{DynamicResource A_FZMaterialPresetGlass_Standard}" />
                <Setter Property="Height" Value="36" />
            </Style>
'''

DPAD_SELECT = ('<local:FZMaterial ImageSource="{StaticResource A_BitmapImage_ControllerButton_DPadRight}" '
               'Width="28" Height="28" Color="White" />')

LABEL = '''<StackPanel Orientation="Horizontal" HorizontalAlignment="Center">
                        <local:FZMaterial x:Name="ANNAMic" Width="0" Height="0" Visibility="Collapsed" />
                        <local:FZMaterial ImageSource="{StaticResource FH2ANNA_Icon_Open}" Width="25" Height="25" Margin="0,0,6,0" Color="White" VerticalAlignment="Center" />
                        <local:FZMaterial ImageSource="/ForzaUI;component/Textures/Data_Bound/FH2ANNA/ANNA_Logo.png" Width="59" Height="18" VerticalAlignment="Center" />
                    </StackPanel>'''


def sub(text, old, new, count=1):
    found = text.count(old)
    if found != count:
        raise ValueError(f'expected {count} of {old[:70]!r}, found {found}')
    return text.replace(old, new)

# ---- the list's rows ---------------------------------------------------------------------------
# FH3 cuts the rows' left ends round the minimap with an opacity mask; on FH2's HUD (drawn tilted)
# a masked element is drawn stretched across the screen, and clips aren't kept. So each row's
# background is a Path, its left end the ring's arc where the row is beside the ring: the row's
# place comes from its index (the list's AlternationIndex) and the list's length (OptionsList's
# Count: FH2's ItemsControl has no Items),
# the list being centred on the ring's centre. Coordinates from the ring's centre.
RING_R    = 153        # the cut: the ring's outer edge (143) and FH3's 10 between
LIST_LEFT = 72         # the rows start here, under the ring (the cut makes their left ends), the
                       # text (right-aligned in 214) then starting 15 clear of the cut
ROW_W     = 318        # a row's background (FH3's), then 3 and the d-pad's 36
ROW_H     = 34
LIST_UP   = 5          # the list's middle this far above the ring's centre
ARC_UP    = 5          # the rows' arc cut: its centre this far above the ring's centre
DPAD      = ROW_H      # the d-pad's square, beside the row (3 between)
PITCH     = 37         # a row and its 1.5 + 1.5 margins
MAX_ROWS  = 14
SHOWN     = 6          # FH2 ANNA's window (src/fh2anna.cpp kRows): when it scrolls, the list is
                       # these and a "more" row above and below, those MORE_H high
TITLE_UP  = 8          # the list's first item is its header, no height at all: a list opened from
                       # another has its title there (its text), drawn this far above the rows below
                       # it; when the list scrolls and the "more" row above the entries shows nothing
                       # (at its top: FH2 ANNA marks the header selected then), over that row instead
MORE_H    = 22
ARROW     = '/ForzaUI;component/Textures/Anthem/SocialFeedIcons/Scroll_Arrow.png'   # the pause menu's (pointing down)
BACK      = '#B0383838'   # FH3's rows: grey, see-through
THEME     = '#FFF4F444'   # FH2's HUD yellow (its gear box's; FnFNitro's Original theme), for FH3's cyan:
                          # the highlighted row, the d-pad's square, the minimap's glowing edge
ON_THEME  = '#FF1A1A1A'   # text on it, and the square the d-pad's picture is on


def left_edge(y):
    """Where a row's background starts at height y (from the ring's centre), from its own left."""
    if abs(y) >= RING_R:
        return 0.0
    return max(0.0, (RING_R * RING_R - y * y) ** 0.5 - LIST_LEFT)


def heights(count):
    """The rows' heights: the header (none), then the entries (and, scrolling, the "more" rows)."""
    rows = count - 1
    if rows == SHOWN + 2:
        return [0, MORE_H] + [ROW_H] * SHOWN + [MORE_H]
    return [0] + [ROW_H] * rows


def pitch(height):
    return height + 3 if height else 0   # a row and its margins; the header has neither


def row_shape(count, index):
    h = heights(count)
    top = ARC_UP - LIST_UP - sum(pitch(x) for x in h) / 2 + sum(pitch(x) for x in h[:index]) + 1.5
    height = h[index]
    steps = 12
    pts = [(left_edge(top + height * k / steps), height * k / steps) for k in range(steps + 1)]
    d = 'M%.1f,0 L%d,0 L%d,%d' % (pts[0][0], ROW_W, ROW_W, height)
    for x, y in reversed(pts):
        d += ' L%.1f,%.1f' % (x, y)
    return d + ' Z'


def more(text, arrow):
    """A row FH2 ANNA adds above or below the entries: there are more that way (arrow: the arrow
    to show), or, at an end with no more, nothing (arrow None)."""
    t = ('                    <DataTrigger Binding="{Binding Command}" Value="' + text + '">\n'
         '                        <Setter TargetName="Row" Property="Height" Value="' + str(MORE_H) + '" />\n'
         '                        <Setter TargetName="Back" Property="Fill" Value="#60383838" />\n'
         '                        <Setter TargetName="Label" Property="Visibility" Value="Collapsed" />\n')
    if arrow:
        t += '                        <Setter TargetName="' + arrow + '" Property="Visibility" Value="Visible" />\n'
    else:
        t += '                        <Setter TargetName="Back" Property="Visibility" Value="Hidden" />\n'
    return t + '                    </DataTrigger>\n'


def arrow(name, up):
    """The pause menu's scroll arrow (its texture points down; the pause menu turns it for up)."""
    turn = (' RenderTransformOrigin="0.5,0.5">\n'
            '                        <local:FZMaterial.RenderTransform><RotateTransform Angle="180"/></local:FZMaterial.RenderTransform>\n'
            '                    </local:FZMaterial>\n') if up else ' />\n'
    return ('                    <local:FZMaterial x:Name="' + name + '" Visibility="Collapsed" ImageSource="' + ARROW + '" '
            'Width="34" Height="16" HorizontalAlignment="Left" VerticalAlignment="Center" '
            'Margin="' + str(ROW_W // 2 - 17) + ',0,0,0"' + turn)


# ANNA listening (voice: "ANNA" heard): awake, the ring lit, its one row FH2 ANNA gives this
# text, drawn as nothing at all.
LISTENING = ('                    <DataTrigger Binding="{Binding Command}" Value="FH2ANNA_LISTENING">\n'
             '                        <Setter TargetName="Row" Property="Height" Value="0" />\n'
             '                        <Setter TargetName="Back" Property="Visibility" Value="Hidden" />\n'
             '                        <Setter TargetName="Label" Property="Visibility" Value="Collapsed" />\n'
             '                        <Setter TargetName="Dpad" Property="Visibility" Value="Hidden" />\n'
             '                    </DataTrigger>\n')

# ANNA driving (autodrive on): awake, the ring lit, as FH3's, its one row drawn as nothing; for its
# first seconds that row is a prompt instead, at the bottom of the screen where the game's own
# ("Press A to...", HUDPressButtonControl) are, centred on the screen.
# Where that row is: the control is at 54, 92 from the HUD's bottom left (A_Hud.xaml), its
# LayoutRoot ROOT from that, the list's Canvas OPTIONS from that; with the header (no height) and
# one 0-high row (1.5 above and below), the list's middle, LIST_UP above the Canvas, is the row's top.
SCREEN_W     = 1920
HUD_LEFT, HUD_BOTTOM           = 54, 92
ROOT_LEFT, ROOT_BOTTOM         = 59, -30
OPTIONS_LEFT, OPTIONS_BOTTOM   = 146, 135
ROW_X = HUD_LEFT + ROOT_LEFT + OPTIONS_LEFT + LIST_LEFT            # from the screen's left
ROW_Y = HUD_BOTTOM + ROOT_BOTTOM + OPTIONS_BOTTOM + LIST_UP        # from the screen's bottom
HINT_W, HINT_H = 1600, 60
HINT_Y   = 88          # the prompt's middle, from the screen's bottom: level with ANNA's label
HINT_PT  = 40
HINTS    = [('FH2ANNA_HINT_CINEMATIC', 'HintCinematic', 'FH2ANNA_Icon_Cinematic', 'CINEMATIC'),
            ('FH2ANNA_HINT_ANNA', 'HintAnna', 'FH2ANNA_Icon_Open', 'ANNA')]


def awake(text, panel=None):
    """ANNA awake for autodrive: its row drawn as nothing (or as the prompt `panel`), and the
    "Auto Drive On" tag on the minimap."""
    t = ('                    <DataTrigger Binding="{Binding Command}" Value="' + text + '">\n'
         '                        <Setter TargetName="Row" Property="Height" Value="0" />\n'
         '                        <Setter TargetName="Back" Property="Visibility" Value="Hidden" />\n'
         '                        <Setter TargetName="Label" Property="Visibility" Value="Collapsed" />\n'
         '                        <Setter TargetName="Dpad" Property="Visibility" Value="Hidden" />\n')
    for name in [panel, 'AutoDriveOn']:
        if name:
            t += '                        <Setter TargetName="' + name + '" Property="Visibility" Value="Visible" />\n'
    return t + '                    </DataTrigger>\n'


# The "Auto Drive On" tag: on the minimap's ring, just above ANNA's label (its middle 145 from the
# control's left, 36 high from its bottom), in the row's coordinates (its top left: the ring's
# centre, 146 and 135 from the control's bottom left, then LIST_LEFT right and LIST_UP up).
TAG_W, TAG_H = 400, 34
TAG_BOTTOM   = 39      # from the control's bottom: the label's 36 and 3 between
TAG_BACK     = '#33383838'   # the rows' grey at 20%: nearly see-through


def autodrive_tag():
    left = 145 - (146 + LIST_LEFT) - TAG_W / 2
    top = (135 + LIST_UP) - TAG_BOTTOM - TAG_H
    text = lambda token, colour: ('                              <local:FZTextBlock Text="' + token + '" Style="{StaticResource FH2ANNA_Text}" '
                                  'FontSize="25" Color="' + colour + '" Margin="4,-1,4,1" VerticalAlignment="Center" />\n')
    return ('                    <Canvas Width="0" Height="0" HorizontalAlignment="Left" VerticalAlignment="Top">\n'
            '                        <Grid x:Name="AutoDriveOn" Visibility="Collapsed" Canvas.Left="%g" Canvas.Top="%g" Width="%d" Height="%d">\n' % (left, top, TAG_W, TAG_H) +
            '                          <Border Background="' + TAG_BACK + '" CornerRadius="9" Padding="8,0,8,0" HorizontalAlignment="Center">\n'
            '                            <StackPanel Orientation="Horizontal" VerticalAlignment="Center">\n' +
            text('FH2ANNA_TEXT_AUTODRIVE', '{StaticResource A_Color_HUD_Text_NoBackground}') + text('FH2ANNA_TEXT_ON', '{StaticResource A_Color_Minimap_EdgeActive}') +
            '                            </StackPanel>\n'
            '                          </Border>\n'
            '                        </Grid>\n'
            '                    </Canvas>\n')


def hint_panels():
    """The prompts: text, the button's picture, text. FH2 ANNA puts the text in, in the game's
    language, as it loads this (src/fh2anna.cpp, hint_texts): FH2ANNA_TEXT_<name>_BEFORE / _AFTER."""
    left = SCREEN_W / 2 - ROW_X - HINT_W / 2
    top = ROW_Y - HINT_Y - HINT_H / 2
    text = lambda token: ('                            <local:FZTextBlock Text="' + token + '" Style="{StaticResource FH2ANNA_Text}" '
                          'FontSize="' + str(HINT_PT) + '" Color="{StaticResource A_Color_HUD_Text_NoBackground}" VerticalAlignment="Center" />\n')
    t = '                    <Canvas Width="0" Height="0" HorizontalAlignment="Left" VerticalAlignment="Top">\n'
    for _, panel, icon, name in HINTS:
        t += ('                        <Grid x:Name="' + panel + '" Visibility="Collapsed" Canvas.Left="%g" Canvas.Top="%g" '
              'Width="%d" Height="%d">\n' % (left, top, HINT_W, HINT_H) +
              '                          <StackPanel Orientation="Horizontal" HorizontalAlignment="Center" VerticalAlignment="Center">\n' +
              text('FH2ANNA_TEXT_' + name + '_BEFORE') +
              '                            <local:FZMaterial ImageSource="{StaticResource ' + icon + '}" Width="' + str(HINT_PT) + '" '
              'Height="' + str(HINT_PT) + '" Margin="10,0,10,0" Color="White" VerticalAlignment="Center" />\n' +
              text('FH2ANNA_TEXT_' + name + '_AFTER') +
              '                          </StackPanel>\n'
              '                        </Grid>\n')
    return t + '                    </Canvas>\n'


# Autodrive's cinematic camera (FH2 ANNA's display mode 4 while it's on, src/fh2anna.cpp): the
# HUD's controls hide (the HUD page's, src/anna_xaml.cpp, which has the letterbox; ANNA's own here).
CINEMATIC = 4


def hide_when_cinematic(t, key):
    """ANNA's parts with this style hide while the cinematic camera's on, too."""
    start = t.index('<Style x:Key="' + key + '"')
    at = t.index('<Style.Triggers>', start) + len('<Style.Triggers>')
    return t[:at] + ('\n                    <DataTrigger Binding="{Binding AnnaDisplayMode}" Value="%d">\n'
                     '                        <Setter Property="Visibility" Value="Collapsed" />\n'
                     '                    </DataTrigger>' % CINEMATIC) + t[at:]


def row_template():
    index = '{Binding (ItemsControl.AlternationIndex), RelativeSource={RelativeSource FindAncestor, AncestorType={x:Type ListBoxItem}}}'
    count = '{Binding DataContext.OptionsList.Count, RelativeSource={RelativeSource FindAncestor, AncestorType=UserControl}}'
    # the header (the first item): no height, nothing of a row, its text (a title, or "") above the rows
    triggers = ('                    <DataTrigger Binding="' + index + '" Value="0">\n'
                '                        <Setter TargetName="Row" Property="Height" Value="0" />\n'
                '                        <Setter TargetName="Row" Property="Margin" Value="0" />\n'
                '                        <Setter TargetName="Back" Property="Visibility" Value="Hidden" />\n'
                '                        <Setter TargetName="Label" Property="Visibility" Value="Collapsed" />\n'
                '                        <Setter TargetName="Dpad" Property="Visibility" Value="Hidden" />\n'
                '                        <Setter TargetName="TitleArea" Property="Visibility" Value="Visible" />\n'
                '                    </DataTrigger>\n'
                # scrolling, at the top: the "more" row between the header and the entries shows
                # nothing, and the title goes over it (FH2 ANNA marks the header selected then)
                '                    <MultiDataTrigger>\n'
                '                        <MultiDataTrigger.Conditions>\n'
                '                            <Condition Binding="' + count + '" Value="' + str(SHOWN + 3) + '"/>\n'
                '                            <Condition Binding="' + index + '" Value="0"/>\n'
                '                            <Condition Binding="{Binding IsSelected}" Value="True"/>\n'
                '                        </MultiDataTrigger.Conditions>\n'
                '                        <Setter TargetName="Title" Property="Canvas.Bottom" Value="' + str(TITLE_UP - pitch(MORE_H)) + '" />\n'
                '                    </MultiDataTrigger>\n')
    for n in range(1, MAX_ROWS + 1):
        for i in range(1, n):
            triggers += ('                    <MultiDataTrigger>\n'
                         '                        <MultiDataTrigger.Conditions>\n'
                         '                            <Condition Binding="' + count + '" Value="' + str(n) + '"/>\n'
                         '                            <Condition Binding="' + index + '" Value="' + str(i) + '"/>\n'
                         '                        </MultiDataTrigger.Conditions>\n'
                         '                        <Setter TargetName="Back" Property="Data" Value="' + row_shape(n, i) + '"/>\n'
                         '                    </MultiDataTrigger>\n')
    return ('<DataTemplate x:Key="A_DataTemplate_SpeechItem">\n'
            '                <Grid x:Name="Row" Width="' + str(ROW_W + 3 + DPAD) + '" Height="' + str(ROW_H) + '" Margin="0,1.5,0,1.5">\n'
            '                    <Path x:Name="Back" HorizontalAlignment="Left" VerticalAlignment="Top" Fill="' + BACK + '" '
            'Data="M0,0 L' + str(ROW_W) + ',0 L' + str(ROW_W) + ',' + str(ROW_H) + ' L0,' + str(ROW_H) + ' Z"/>\n'
            '                    <local:FZTextBlock x:Name="Label" Text="{Binding Command}" Style="{StaticResource FH2ANNA_Text}" FontSize="25" Margin="0,-1,' + str(DPAD + 11) + ',1" '
            'Width="{StaticResource A_Double_SpeechOptionWidth}" VerticalAlignment="Center" HorizontalAlignment="Right" />\n'
            # "there's more that way" rows (FH2 ANNA gives them these texts): the pause menu's arrow,
            # on a shorter, fainter row
            + arrow('MoreUp', True) + arrow('MoreDown', False) +
            # the header's title: above the rows, where the entries' text starts, in the highlight's colour
            '                    <Canvas x:Name="TitleArea" Visibility="Collapsed" Width="' + str(ROW_W + 3 + DPAD) + '" Height="0" VerticalAlignment="Top">\n'
            '                        <local:FZTextBlock x:Name="Title" Text="{Binding Command}" Style="{StaticResource FH2ANNA_Text}" FontSize="23" '
            'Color="' + THEME + '" Canvas.Bottom="' + str(TITLE_UP) + '" Canvas.Right="' + str(DPAD + 11) + '" '
            'Width="{StaticResource A_Double_SpeechOptionWidth}" />\n'
            '                    </Canvas>\n' +
            # the button that chooses, on a dark square (FH2's d-pad is white: on the yellow it'd vanish)
            '                    <local:FZMaterial x:Name="Dpad" HorizontalAlignment="Right" Width="' + str(DPAD) + '" Height="' + str(DPAD) + '" '
            'Color="' + ON_THEME + '" Visibility="Hidden">\n'
            '                        <local:FZMaterial ImageSource="{StaticResource FH2ANNA_Icon_Choose}" '
            'Width="28" Height="28" Color="White" />\n'
            '                    </local:FZMaterial>\n' + hint_panels() + autodrive_tag() +
            '                </Grid>\n'
            '                <DataTemplate.Triggers>\n'
            '                    <DataTrigger Binding="{Binding IsSelected}" Value="True">\n'
            '                        <Setter TargetName="Back" Property="Fill" Value="' + THEME + '" />\n'
            '                        <Setter TargetName="Label" Property="Color" Value="' + ON_THEME + '" />\n'
            '                        <Setter TargetName="Dpad" Property="Visibility" Value="Visible" />\n'
            '                    </DataTrigger>\n'
            # disabled entries ("FH2ANNA_OFF:" and their label): FH2 ANNA puts their triggers here as
            # it loads this (src/fh2anna.cpp, disabled_triggers), after the highlight's, so they win
            '                    <!--FH2ANNA_OFF-->\n' + more('FH2ANNA_MORE_UP', 'MoreUp') +
            more('FH2ANNA_MORE_DOWN', 'MoreDown') + more('FH2ANNA_MORE_NONE', None) + LISTENING + awake('FH2ANNA_AWAKE') +
            ''.join(awake(text, panel) for text, panel, _, _ in HINTS) + triggers +
            '                </DataTemplate.Triggers>\n'
            '            </DataTemplate>')


def port(t):
    for name in TEXTURES:
        t = t.replace(f'/ForzaUI;component/Textures/Anthem/HUD/{name}.png',
                      f'/ForzaUI;component/Textures/Data_Bound/FH2ANNA/{name}.png')
    t = sub(t, '        <ResourceDictionary>\n', '        <ResourceDictionary>\n' + RESOURCES)
    # FH3's cyan is FH2's yellow
    t = sub(t, '<Color x:Key="A_Color_Minimap_EdgeActive">#FF00d2ff</Color>', '<Color x:Key="A_Color_Minimap_EdgeActive">' + THEME + '</Color>')
    t = sub(t, '<Color x:Key="A_Color_ItemSelected">#FF00c6ff</Color>', '<Color x:Key="A_Color_ItemSelected">' + THEME + '</Color>')
    t = sub(t, '<Color x:Key="A_Color_ItemHighlighted">#FF00c6ff</Color>', '<Color x:Key="A_Color_ItemHighlighted">' + THEME + '</Color>')
    t = t.replace('{StaticResource A_FZMaterialPresetAdditive}', '{StaticResource FH2ANNA_Preset_Plain}')
    t = t.replace('{StaticResource A_Style_FZTextBlock_HudMedium_Tight}', '{StaticResource FH2ANNA_Text}')
    t = t.replace('{StaticResource A_Style_FZTextBlock_HudMedium}', '{StaticResource FH2ANNA_Text}')
    t = sub(t, 'BasedOn="{StaticResource FH2ANNA_Text}">\n                <Setter Property="FontSize" Value="26" />',
            'BasedOn="{StaticResource A_Style_FZTextBlock_HudMedium}">\n                <Setter Property="FontSize" Value="26" />')
    t = t.replace('{StaticResource A_Style_FZMaterial_GlassHudItem}', '{StaticResource FH2ANNA_GlassHudItem}')
    t = t.replace('{StaticResource A_Double_HUDMiniLeaderboard_RowHeight}', '36')
    # the list items: FH2's d-pad picture, FH2's highlight
    t = re.sub(r'<local:ControllerButtonControl CommandOverride="INPUTCMD_ANNA_SELECT"[^>]*/>', DPAD_SELECT, t)
    t = sub(t, 'Binding="{Binding IsHighlighted}"', 'Binding="{Binding IsSelected}"')
    # the label
    start = t.index('<Grid HorizontalAlignment="Center" Margin="-3,0,0,0">')
    end = t.index('</Grid>', t.index('A_Style_FrameworkElement_ANNAUnavailable', start)) + len('</Grid>')
    t = t[:start] + LABEL + t[end:]
    # the minimap
    t = sub(t, '<local:MapCustomRenderElement Alpha="1.0" MapId="default" x:Name="mapCustomRenderElement">',
            '<local:MapCustomRenderElement Alpha="1.0" MapId="default" MapProfile="hud" x:Name="mapCustomRenderElement">')
    t = re.sub(r'\s*ElementToTrack="\{Binding ElementToTrack[^"]*\}"', '', t)
    # the list's circular mask on a Grid: FH2's FZMaterial scales its content to fill itself
    t = sub(t, '<Style x:Key="A_Style_FZMaterial_ANNAOptions_Mask" TargetType="{x:Type local:FZMaterial}">',
            '<Style x:Key="A_Style_FZMaterial_ANNAOptions_Mask" TargetType="{x:Type Grid}">')
    t = sub(t, '<Setter Property="Margin" Value="31,0,0,45"/>\n                <Setter Property="Color" Value="Transparent"/>',
            '<Setter Property="Margin" Value="31,0,0,45"/>')
    opening = '<local:FZMaterial Style="{StaticResource A_Style_FZMaterial_ANNAOptions_Mask}">'
    start = t.index(opening)
    t = t[:start] + '<Grid Style="{StaticResource A_Style_FZMaterial_ANNAOptions_Mask}">' + t[start + len(opening):]
    end = t.index('</local:FZMaterial>', t.index('x:Name="ANNAMain"'))
    t = t[:end] + '</Grid>' + t[end + len('</local:FZMaterial>'):]
    # FH2's Decorator scales its child to fill it: plain Grids instead
    t = t.replace('<Decorator ', '<Grid ').replace('<Decorator>', '<Grid>').replace('</Decorator>', '</Grid>')
    # The list: FH3 sizes it with styles that switch on the display mode (a 430-wide box, masked
    # round the minimap), which FH2 doesn't apply, so it fills the whole HUD. Instead, FH3's list
    # as it is, in a Canvas at the ring's centre (the arcs are 6, 10, 6, 4 inside the 300 minimap,
    # itself -4 left and -12 below: the centre is 146 from the left, 135 from the bottom, the
    # radius 142), starting 12 inside the ring's right edge, its middle on the centre; FH3's rows
    # are 268 wide (the text right-aligned in them) and the d-pad's square 39 more.
    start = t.index('<Grid x:Name="HUDAnnaControl_ANNAOptions"')
    end = t.index('<Grid x:Name="HUDAnnaControl_EdgeGlow"')
    block = t[start:end]
    a = block.index('<local:CommonListBox')
    b = block.index('</local:CommonListBox>') + len('</local:CommonListBox>')
    listbox = block[a:b]
    t = t[:start] + ('<Canvas x:Name="HUDAnnaControl_ANNAOptions" Style="{StaticResource A_Style_FrameworkElement_HideIfMapOnly}" '
                     'HorizontalAlignment="Left" VerticalAlignment="Bottom" Width="0" Height="0" '
                     'Margin="%d,0,0,%d">\n' % (OPTIONS_LEFT, OPTIONS_BOTTOM) +
                     '            <Grid Canvas.Left="' + str(LIST_LEFT) + '" Canvas.Top="' + str(-500 - LIST_UP) + '" Height="1000">\n'
                     '                <StackPanel x:Name="ANNAMain" Orientation="Vertical" VerticalAlignment="Center" Width="' + str(ROW_W + 3 + DPAD) + '">\n'
                     '                    ' + listbox.replace('<local:CommonListBox ', '<local:CommonListBox AlternationCount="' + str(MAX_ROWS) + '" ', 1) + '\n'
                     '                </StackPanel>\n'
                     '            </Grid>\n'
                     '        </Canvas>\n\n        ') + t[end:]
    # The rows: FH3's look, each row's background a shape whose left end is the ring's arc.
    a = t.index('<DataTemplate x:Key="A_DataTemplate_SpeechItem">')
    b = t.index('</DataTemplate>', a) + len('</DataTemplate>')
    t = t[:a] + row_template() + t[b:]
    # sounds
    t = t.replace('HZ3_SFX_HUD_ANNA_Select_01', 'HZ2_SFX_HUD_ANNA_Wake_01').replace('HZ3_SFX_HUD_ANNA_', 'HZ2_SFX_HUD_ANNA_')
    # where FH3's HUD puts it
    t = sub(t, '<Grid x:Name="LayoutRoot" Style="{StaticResource A_Style_FrameworkElement_HideIfOff}">',
            '<Grid x:Name="LayoutRoot" Style="{StaticResource A_Style_FrameworkElement_HideIfOff}" '
            'Margin="%d,0,0,%d">' % (ROOT_LEFT, ROOT_BOTTOM))
    # the cinematic camera: ANNA's parts hidden, and the mode on the control's Tag, for the HUD page
    # (src/anna_xaml.cpp: its controls hide by it, and it has the letterbox)
    t = hide_when_cinematic(t, 'A_Style_FrameworkElement_HideIfMapOnly')
    t = hide_when_cinematic(t, 'A_Style_FrameworkElement_HideIfANNAOnly')
    t = sub(t, '    x:Name="UserControl"\n', '    x:Name="UserControl"\n    Tag="{Binding AnnaDisplayMode}"\n')
    return t


def main():
    fh3 = sys.argv[1] if len(sys.argv) > 1 else r'D:\Games\FH2\Opus_1.0.125.0_x64__8wekyb3d8bbwe'
    text = zipfile.ZipFile(os.path.join(fh3, 'Mount', 'media', 'UI.zip')).read('Controls/HUDAnnaControl.xaml').decode('utf-8-sig')
    text = text.replace('\r\n', '\n')
    out = port(text).replace('\n', '\r\n')
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    path = os.path.join(here, 'assets', 'HUDAnnaControl.xaml')
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(out)
    print(path, len(out))


if __name__ == '__main__':
    main()
