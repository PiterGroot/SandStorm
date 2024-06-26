#include "ElementRules.h"

float aplhaRandomness = 200;

ElementRules::ElementRules()
{
    //Adding new cells steps:
    //  1. Add a new element type (Elements::Element)
    //  2. Add a hotkey check for switching to new cell
    //  3. Add string -> enum conversion for UI label
    //  4. Add rules for new cell
    //  5. Add color for new cell
    //  6. Bind element to its ruleset
    //  (optional) 7. Bind raw pixel color to cell element for imageimporter
    //  (optional) 8. Add custom cell behaviour to for interaction.

    // Initialize rule sets
    sandRules =  { DOWN, SIDE_DOWN };
    waterRules = { DOWN, SIDE, SIDE_DOWN };
    smokeRules = { UP, SIDE_UP, SIDE };
    lavaRules =  { DOWN, SIDE, SIDE_DOWN };
    fireRules =  { UP, SIDE_UP, SIDE };
    
    woodRules =  { STAY };
    stationaryFireRules =  { STAY };

    // Initialize getRuleSet map
    getRuleSet = {
        { Element::Elements::SAND,                        sandRules  },
        { Element::Elements::WATER,                       waterRules },
        { Element::Elements::SMOKE,                       smokeRules },
        { Element::Elements::LAVA,                        lavaRules  },
        { Element::Elements::WOOD,                        woodRules  },
        { Element::Elements::FIRE,                        fireRules  },
        { Element::Elements::STATIONARY_FIRE,   stationaryFireRules  }
    };

    // Initialize cell base color values
    cellColorValues = {
        { Element::Elements::SAND,       Color(255, 255, 0, 255)   },
        { Element::Elements::WATER,      Color(0, 0, 255, 255)     },
        { Element::Elements::WALL,       Color(255, 255, 255, 255) },
        { Element::Elements::SMOKE,      Color(150, 150, 150, 255) },
        { Element::Elements::LAVA,       Color(255, 77, 28, 255)   },
        { Element::Elements::WOOD,       Color(130, 65, 0, 255)    },
    };

    // Initialize ruleValues map
    ruleValues = {
        { UP,         Vector2(0, -1) },
        { DOWN,       Vector2(0, 1)  },
    };
}

//Returns a randomized color value based on input element
Color ElementRules::GetCellColor(Element::Elements element)
{
    if (element == Element::Elements::UNOCCUPIED) return BLACK;
    if (element == Element::Elements::OBSIDIAN)   return BLACK;
    
    bool isFireElement = element == Element::Elements::STATIONARY_FIRE || element == Element::Elements::FIRE;
    if (isFireElement) //special colors for fire
    {
        int randColor = GetRandomValue(1, 5);
        switch (randColor)
        {
            case 1: return Color(156, 43, 17, 255);
            case 2: return Color(255, 106, 0, 255);
            case 3: return Color(127, 0, 0, 255);
            case 4: return Color(255, 151, 0, 255);
            case 5: return Color(127, 51, 0, 255);
        }
    }
    
    Color baseColor = cellColorValues[element];
    int randAlpha = GetRandomValue(aplhaRandomness, 255); // randomize alpha
    
    return Color(baseColor.r, baseColor.g, baseColor.b, randAlpha);
}