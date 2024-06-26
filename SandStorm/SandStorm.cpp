#include "SandStorm.h"

SandStorm* SandStorm::instance = nullptr;

constexpr auto WIDTH = 512;
constexpr auto HEIGHT = 512;

constexpr int size = WIDTH * HEIGHT;

Color pixels[size];
SandStorm::CellInfo map[size];

SandStorm::SandStorm() //constructor
{
    instance = this;
    cursor = LoadTexture("Textures/cursor.png");

    screenImage = GenImageColor(WIDTH, HEIGHT, UNOCCUPIED_CELL);
    screenTexture = LoadTextureFromImage(screenImage);

    for (int i = 0; i < WIDTH * HEIGHT; i++) //set texture background to black
    {
        pixels[i] = UNOCCUPIED_CELL;
    }

    screenImage.data = pixels; //update image with black background
    elementRules = new ElementRules(); //create cell rules ref
    inputHandler = new InputHandler(Vector2(WIDTH / 2, HEIGHT / 2)); //create InputHandler ref
    imageImporter = new ImageImporter(WIDTH); //create ImageImporter ref

    srand(time(0)); //set randoms seed
    InitAudioDevice();

    removeAutoSFX =  LoadSound("Resources/Audio/removeAuto.wav");
    resetSFX = LoadSound("Resources/Audio/reset.wav");
    placeAutoSFX = LoadSound("Resources/Audio/placeAuto.wav");
}

SandStorm::~SandStorm() //deconstructor
{
    delete elementRules;
    delete inputHandler;
}

//Main update loop
void SandStorm::Update(float deltaTime)
{
    HandleCellSwitching();

    mousePosition = GetMousePosition();
    inputHandler->OnUpdate(mousePosition); //update general input checks

    //Try update all active cells
    if (shouldUpdate)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            for (int y = 0; y < HEIGHT - 1; y++)
            {
                UpdateCell(x, y);
            }
        }
    }
    UpdateTexture(screenTexture, pixels); //NOTE: does texture need to be updated every frame?
}

//Main render loop
void SandStorm::Render()
{
    BeginDrawing();
    ClearBackground(BLACK);

    DrawTexture(screenTexture, 0, 0, WHITE);

    int halfBrushSize = brushSize / 2;
    float offset = brushSize * .2f;

    Rectangle srcRec = Rectangle(0, 0, cursor.width, cursor.height);
    Rectangle dstRec = Rectangle(mousePosition.x - halfBrushSize, mousePosition.y - halfBrushSize, cursor.width * offset, cursor.height * offset);
    DrawTexturePro(cursor, srcRec, dstRec, Vector2(brushSize, brushSize), 0, WHITE);
   
    if (showHudInfo) 
    {
        DrawFPS(0, 0); //draw fps
        DrawText(GetElementString().c_str(), 0, 24, 24, GREEN); //draw current element and brush size
        DrawText(shouldUpdate ? "Active" : "Paused", 256 - 45, 0, 24, GREEN); //draw update state label
        DrawText(imageImporter->currentImportedImage.c_str(), 0, HEIGHT - 16, 16, GREEN); //draw update state label
    }

    //Update auto cell manipulators at end of frame
    for (const auto& manipulator : SandStorm::autoManipulators)
    {
        float scale = manipulator.position.z * 2;
        float xPos = manipulator.position.x;
        float yPos = manipulator.position.y;

        if(showHudInfo) DrawRectangleLines(xPos - manipulator.position.z, yPos - manipulator.position.z, scale, scale, manipulator.mode ? GREEN : RED);
        if(shouldUpdate) ManipulateCell(manipulator.mode, xPos, yPos, manipulator.placeElement, manipulator.position.z);
    }

    EndDrawing();
    if (skipTimerActive)
        shouldUpdate = false;
}

//Update cell based on its rules
void SandStorm::UpdateCell(int x, int y)
{
    int oldIndex = x + WIDTH * y;
    int currentCell = map[oldIndex].type;

    if (currentCell == 0 || currentCell == 3) //skip air (empty)/wall cells
        return;

    if (map[oldIndex].isUpdated) //skip cell if it has already beed updated this frame
    {
        map[oldIndex].isUpdated = false;
        return;
    }

    Element::Elements currentElement = static_cast<Element::Elements>(currentCell);
    auto& cellRuleSet = elementRules->getRuleSet[currentElement]; //get the right ruleset based on cell element type

    for (const auto& rule : cellRuleSet) //loop through all rules of the ruleset
    {
        int xPos = 0;
        int yPos = 0;

        if (elementRules->ruleValues.contains(rule))
        {
            Vector2 checkVector = elementRules->ruleValues[rule];
            xPos = checkVector.x;
            yPos = checkVector.y;
        }
        else if (rule == ElementRules::Rules::SIDE)
        {
            xPos = GetRandomValue(0, 1) == 0 ? -1 : 1;
        }
        else if (rule == ElementRules::Rules::SIDE_DOWN)
        {
            xPos = GetRandomValue(0, 1) == 0 ? -1 : 1;
            yPos = 1;
        }
        else if (rule == ElementRules::Rules::SIDE_UP)
        {
            xPos = GetRandomValue(0, 1) == 0 ? -1 : 1;
            yPos = -1;
        }

        if (IsOutOfBounds(xPos + x, yPos + y)) //check if next desired position is out of bounds
            continue;

        int newIndex = (x + xPos) + WIDTH * (y + yPos);
        int newIndexType = map[newIndex].type;

        if (newIndexType == 0) //try to go to desired postion based on current rule, if next index is empty
        {
            SetCell(oldIndex, Element::Elements::UNOCCUPIED, false);
            SetCell(newIndex, currentElement);

            if (currentCell == 9) {

                map[newIndex].updateTick = map[oldIndex].updateTick;
                map[newIndex].lifeTime = map[oldIndex].lifeTime;
                map[newIndex].updateTick++;

                map[oldIndex].updateTick = 0;
                map[oldIndex].lifeTime = 0;

                if (map[newIndex].updateTick == map[newIndex].lifeTime)
                {
                    map[newIndex].updateTick = 0;
                    map[newIndex].lifeTime = 0;

                    if (!GetChance(80)) SetCell(newIndex, Element::Elements::UNOCCUPIED, true);
                    else SetCell(newIndex, Element::Elements::SMOKE, true);

                    break;
                }
            }
            break;
        }

#pragma region CellInteractions
        //swap sand with water if sand falls on top of water
        if (currentCell == 1 && newIndexType == 2)
        {
            SwapCell(oldIndex, newIndex, Element::Elements::SAND, Element::Elements::WATER);
            break;
        }

        //swap sand with smoke if sand falls on top of water
        if (currentCell == 1 && newIndexType == 4)
        {
            SwapCell(oldIndex, newIndex, Element::Elements::SAND, Element::Elements::SMOKE);
            break;
        }

        //swap sand with fire if sand falls on top of water
        if (currentCell == 1 && newIndexType == 9)
        {
            SwapCell(oldIndex, newIndex, Element::Elements::SAND, Element::Elements::FIRE);
            break;
        }

        //create smoke when water or sand touches 
        bool createObsidianFromSand = currentCell == 1 && newIndexType == 5;
        bool createObsidianFromWater = currentCell == 2 && newIndexType == 5;

        if (createObsidianFromSand || createObsidianFromWater)
        {
            SetCell(oldIndex, Element::Elements::SMOKE);
            SetCell(newIndex, Element::Elements::OBSIDIAN);
            break;
        }

        //create obsidian when lava touches sand
        if (currentCell == 5 && newIndexType == 1)
        {
            SetCell(newIndex, Element::Elements::OBSIDIAN);
            break;
        }

        //create fire when lava touches wood
        if (currentCell == 5 && newIndexType == 7) 
        {
            SetCell(newIndex, Element::Elements::STATIONARY_FIRE);
            break;
        }

        //initial wood burning
        if (currentCell == 9) 
        {
            int upIndex = oldIndex - HEIGHT;
            if (map[upIndex].type == 7)
            {
                map[oldIndex].updateTick = 0;
                map[oldIndex].lifeTime = 0;

                SetCell(oldIndex, Element::Elements::UNOCCUPIED, true);
                SetCell(upIndex, Element::Elements::STATIONARY_FIRE, true);
                break;
            }
        }
        
        //fire spreading logic
        if (currentCell == 7)
        {
            int upIndex = oldIndex - HEIGHT;
            int downIndex = oldIndex + HEIGHT;
            int leftIndex = oldIndex - 1;
            int rightIndex = oldIndex + 1;

            if (map[upIndex].type == 8 || map[downIndex].type == 8 || map[leftIndex].type == 8 || map[rightIndex].type == 8)
            {
                map[oldIndex].updateTick++;

                if (map[oldIndex].updateTick == map[oldIndex].lifeTime)
                {
                    map[oldIndex].updateTick = 0;
                    SetCell(oldIndex, Element::Elements::STATIONARY_FIRE, true);
                    break;
                }
            }
        }
        
        //fire despawning
        if (currentCell == 8) 
        {
            map[oldIndex].updateTick++;
            if (map[oldIndex].updateTick == map[oldIndex].lifeTime)
            {
                map[oldIndex].updateTick = 0;
                map[oldIndex].lifeTime = 0;
                
                if (!GetChance(80)) SetCell(newIndex, Element::Elements::UNOCCUPIED, true);
                else SetCell(newIndex, Element::Elements::SMOKE, true);

                break;
            }
        }
#pragma endregion 
    }
}

//Helper method for setting single cells
void SandStorm::SetCell(int index, Element::Elements element, bool markUpdated)
{
    pixels[index] = elementRules->GetCellColor(element);
    map[index].type = element;
    map[index].isUpdated = markUpdated;

    //Initialize dynamic cells with a random life time value
    if (element == Element::Elements::STATIONARY_FIRE) map[index].lifeTime = GetRandomValue(75, 275);
    if (element == Element::Elements::FIRE) map[index].lifeTime = GetRandomValue(25, 100);
    if (element == Element::Elements::WOOD) map[index].lifeTime = GetRandomValue(10, 25);
}

//Helper method for swapping two cells with each other
void SandStorm::SwapCell(int fromIndex, int toIndex, Element::Elements swapA, Element::Elements swapB)
{
    pixels[fromIndex] = elementRules->GetCellColor(swapB);
    pixels[toIndex] = elementRules->GetCellColor(swapA);
    
    map[fromIndex].type = swapB;
    map[toIndex].type = swapA;

    map[fromIndex].isUpdated = true;
    map[toIndex].isUpdated = true;
}

//Placing / destroying cells with mouse
void SandStorm::ManipulateCell(bool state, int xPos, int yPos, Element::Elements placeElement, int overrideBrushSize)
{
    int radius = overrideBrushSize == 0 ? this->brushSize : overrideBrushSize;
    for (int x = -radius; x <= radius; x++)
    {
        for (int y = -radius; y <= radius; y++)
        {
            float distance = sqrt(x * x + y * y);
            if (distance <= radius)
            {
                int index = (x + xPos) + WIDTH * (y + yPos);
                if (IsOutOfBounds(xPos + x, yPos + y)) //check for all brush positions if it is out of bounds
                    continue;

                if (state) //placing cells 
                {
                    //placing fill chance based on current element and brush size
                    float fillChance = (placeElement == Element::Elements::WALL || placeElement == Element::Elements::WOOD) ? cellPlacingNoRandomization : cellPlacingRandomization;
                    if (GetChance(fillChance))
                    {
                        if (map[index].type == 0)
                            SetCell(index, placeElement, false);
                    }
                }
                else //destroying cells
                {
                    if (map[index].type > 0)
                    {
                        SetCell(index, Element::UNOCCUPIED, false);
                        map[index].isUpdated = false;
                        map[index].lifeTime = 0;
                        map[index].updateTick = 0;
                    }
                }
            }
        }
    }
}

//Switching between elements
void SandStorm::HandleCellSwitching()
{
    if (IsKeyDown(KEY_LEFT_CONTROL)) //ignore when holding ctrl (messes with other shortcuts)
        return;

    if (IsKeyPressed(KEY_ONE)) currentElement =   Element::Elements::SAND;
    if (IsKeyPressed(KEY_TWO)) currentElement =   Element::Elements::WATER;
    if (IsKeyPressed(KEY_THREE)) currentElement = Element::Elements::WALL;
    if (IsKeyPressed(KEY_FOUR)) currentElement =  Element::Elements::SMOKE;
    if (IsKeyPressed(KEY_FIVE)) currentElement =  Element::Elements::LAVA;
    if (IsKeyPressed(KEY_SIX)) currentElement =   Element::Elements::WOOD;
    if (IsKeyPressed(KEY_SEVEN))currentElement =  Element::Elements::FIRE;
   
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) //temp debugging shortcut to spawn sand cell
    {
        Element::Elements debugElement = Element::Elements::SAND;
        int index = 256 + WIDTH * 256;
        pixels[index] = elementRules->GetCellColor(debugElement);
        map[index].type = debugElement;
    }
}

//Helper method for clearing the simulation grid
void SandStorm::ResetSim()
{
    std::fill(std::begin(pixels), std::end(pixels), UNOCCUPIED_CELL);
    std::fill(std::begin(map), std::end(map), CellInfo());
 
    imageImporter->currentImportedImage = "";
    autoManipulators.clear();

    PlaySound(SandStorm::instance->resetSFX);
}

//Helper method for creating and exporting screenshots
void SandStorm::ExportScreenShot()
{
    std::filesystem::path directoryPath = GetApplicationDirectory(); //define screenshot path
    directoryPath /= "Screenshots";

    if (!std::filesystem::exists(directoryPath)) //create 'Screenshot' folder if it doesn't exist
        std::filesystem::create_directory(directoryPath);

    //gather timestamp info
    struct tm timeInfo;
    auto currentTime = std::chrono::system_clock::now();
    auto currentTimeT = std::chrono::system_clock::to_time_t(currentTime);
    localtime_s(&timeInfo, &currentTimeT);

    strftime(timeBuffer, sizeof(timeBuffer), "%d-%H-%M-%S-", &timeInfo); //format timestamp based on timeInfo and put it inside timeBuffer

    std::string timeStamp(timeBuffer);
    std::filesystem::path imagePath = directoryPath / (timeStamp + "screenshot.png"); //define file name/path

    //load and export current screen texture 
    Image image = LoadImageFromTexture(screenTexture);
    ExportImage(image, imagePath.string().c_str());
    
    UnloadImage(image);
}

//Checks if given position is outside the window
bool SandStorm::IsOutOfBounds(int posX, int posY)
{
    bool outOfBoundsA = posX >= WIDTH || posY >= HEIGHT;
    bool outOfBoundsB = posX < 0 || posY < 0;

    return outOfBoundsA || outOfBoundsB;
}

//Calculates and returns a chance based on input value
bool SandStorm::GetChance(float input)
{
    return GetRandomValue(0, 100) > input;
}

//Convert current element enum value to string for UI label
std::string SandStorm::GetElementString()
{
    switch (currentElement)
    {
        case 1:  return "Sand " +      std::to_string(brushSize);
        case 2:  return "Water " +     std::to_string(brushSize);
        case 3:  return "Wall " +      std::to_string(brushSize);
        case 4:  return "Smoke " +     std::to_string(brushSize);
        case 5:  return "Lava " +      std::to_string(brushSize);
        case 7:  return "Wood " +      std::to_string(brushSize);
        case 9:  return "Fire " +      std::to_string(brushSize);
        default: return "UNDIFINED " + std::to_string(brushSize);
    }
}