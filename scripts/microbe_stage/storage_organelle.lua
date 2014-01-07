--------------------------------------------------------------------------------
-- A storage organelle class
--------------------------------------------------------------------------------
class 'StorageOrganelle' (Organelle)

-- Constructor
--
-- @param bandwidth
-- The rate of transfer of this organelle
--
-- @param capacity
-- The maximum stored amount
function StorageOrganelle:__init(bandwidth, capacity)
    Organelle.__init(self)
    self.bandwidth = bandwidth
    self.capacity = capacity
    self.compounds = {}
    self.stored = 0
    self.parentIndex = 0
end


function StorageOrganelle:load(storage)
    Organelle.load(self, storage)
    self.bandwidth = storage:get("bandwidth", 10)
    self.capacity = storage:get("capacity", 100)
end


function StorageOrganelle:storage()
    local storage = Organelle.storage(self)
    storage:set("bandwidth", self.bandwidth)
    storage:set("capacity", self.capacity)
    return storage
end

-- Overridded from Organelle:onAddedToMicrobe
function StorageOrganelle:onAddedToMicrobe(microbe, q, r)
    Organelle.onAddedToMicrobe(self, microbe, q, r)
    parentIndex = microbe:addStorageOrganelle(self)
end

-- Overridded from Organelle:onRemovedFromMicrobe
function StorageOrganelle:onRemovedFromMicrobe(microbe, q, r)
    Organelle.onRemovedFromMicrobe(self, microbe, q, r)
    microbe:removeStorageOrganelle(self)
end

--Stores as much of the compound as possible, returning the amount that wouldn't fit
function StorageOrganelle:storeCompound(compoundId, amount)
    local canFit = (self.capacity - self.stored)/CompoundRegistry.getCompoundSize(compoundId)
    local amountToStore = math.min(amount, self.bandwidth, canFit)
    if self.compounds[compoundId] == nil then
        self.compounds[compoundId] = amountToStore
    else
        self.compounds[compoundId] = self.compounds[compoundId] + amountToStore
    end
    self.stored = self.stored + CompoundRegistry.getCompoundSize(compoundId)*amountToStore
    return amount - amountToStore
end

--Ejects as much of the compound as possible, returning how much was ejected
function StorageOrganelle:ejectCompound(compoundId, amount)
    if self.compounds[compoundId] ~= nil then
        local drainAmount = math.min(amount, self.compounds[compoundId], self.bandwidth)
        self.compounds[compoundId] = self.compounds[compoundId] - drainAmount
        self.stored = self.stored - drainAmount
        return drainAmount
    else
        return 0
    end
end


function StorageOrganelle:update(microbe, milliseconds)
    Organelle.update(self, microbe, milliseconds)
    --vacuoles don't do anything... they just... sit there... any ideas what goes here?
end

