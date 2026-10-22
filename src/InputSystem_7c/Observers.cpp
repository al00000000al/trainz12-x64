// InputSystem_7c unit U1: ObserverSubject<T> for the three observer types InputSystem exports (InputObserver,
// InputActionObserver, InputUIExtensionObserver) and the observer bases. The template follows the Jet SDK's
// ObserverSubject.h with TS12's priority: the list is a FastDynamicArray<UncertainObserver, 4> sorted highest
// priority first, and an observer added again with another priority moves. Transcribed from the assembly
// (AddObserver 0x100123f0, RemoveObserver 0x100124a0, UnlockObservers 0x1000c0e0, the copy 0x1000c580).
#include <jet/InputSystem.h>

namespace Jet {

template <class O>
ObserverSubject<O>::ObserverSubject() : observerLock(0)
{
}

template <class O>
ObserverSubject<O>::ObserverSubject(const ObserverSubject& o) : observerLock(0)
{
    observers.AppendChunks(o.observers);
    observerLock = o.observerLock;
}

template <class O>
ObserverSubject<O>::~ObserverSubject()
{
    observers.RemoveAll();
}

// no self-assignment check: assigning a subject to itself empties it (as the original)
template <class O>
ObserverSubject<O>& ObserverSubject<O>::operator=(const ObserverSubject& o)
{
    observers.RemoveAll();
    observers.AppendChunks(o.observers);
    observerLock = o.observerLock;
    return *this;
}

template <class O>
void ObserverSubject<O>::AddObserver(O* observer, int priority)
{
    const UncertainObserver entry(observer, priority);
    typename List::Iterator i = observers.Find(entry);
    if (i.node) {
        if (i.node->priority == priority) {
            if (observerLock) i.node->isValid = true;
            return;
        }
        // another priority: the old entry goes (only flagged while locked) and the new one is sorted in
        if (observerLock)
            i.node->isValid = false;
        else
            observers.Remove(i);
    }
    observers.Insert(entry);
}

template <class O>
void ObserverSubject<O>::RemoveObserver(O* observer)
{
    typename List::Iterator i = observers.Find(UncertainObserver(observer, 0));
    if (!i.node) return;
    if (observerLock)
        i.node->isValid = false;
    else
        observers.Remove(i);
}

template <class O>
bool ObserverSubject<O>::HasObserver(const O* observer) const
{
    typename List::ConstIterator i = observers.Find(UncertainObserver(const_cast<O*>(observer), 0));
    return i.node && i.node->isValid;
}

template <class O>
void ObserverSubject<O>::LockObservers()
{
    ++observerLock;
}

template <class O>
void ObserverSubject<O>::UnlockObservers()
{
    if (--observerLock != 0) return;
    for (typename List::Iterator i = observers.First(); i.node;) {
        if (i.node->isValid)
            ++i;
        else
            i = observers.Remove(i);
    }
}

template <class O>
unsigned long ObserverSubject<O>::Count()
{
    return observers.Count();
}

// First / Last / End take the list's iterators as they are: First does not skip a cleared entry
template <class O>
typename ObserverSubject<O>::ObserverIterator ObserverSubject<O>::FirstObserver()
{
    return ObserverIterator(observers.First(), observers);
}

template <class O>
typename ObserverSubject<O>::ConstObserverIterator ObserverSubject<O>::FirstObserver() const
{
    return ConstObserverIterator(observers.First(), observers);
}

template <class O>
typename ObserverSubject<O>::ObserverIterator ObserverSubject<O>::LastObserver()
{
    return ObserverIterator(observers.Last(), observers);
}

template <class O>
typename ObserverSubject<O>::ConstObserverIterator ObserverSubject<O>::LastObserver() const
{
    return ConstObserverIterator(observers.Last(), observers);
}

template <class O>
typename ObserverSubject<O>::ObserverIterator ObserverSubject<O>::EndObserver()
{
    return ObserverIterator(observers.End(), observers);
}

template <class O>
typename ObserverSubject<O>::ConstObserverIterator ObserverSubject<O>::EndObserver() const
{
    return ConstObserverIterator(observers.End(), observers);
}

template <class O>
int ObserverSubject<O>::GetNumObserverLocks() const
{
    return observerLock;
}

template class ObserverSubject<InputObserver>;
template class ObserverSubject<InputActionObserver>;
template class ObserverSubject<InputUIExtensionObserver>;

// ---- observer bases
InputObserver::InputObserver() {}
InputObserver::InputObserver(const InputObserver&) {}
InputObserver& InputObserver::operator=(const InputObserver&) { return *this; }
InputEvent::Propagation InputObserver::OnInputEvent(const UId&, const InputEvent&) { return InputEvent::PROPAGATE; }

InputActionObserver::InputActionObserver() {}
InputActionObserver::InputActionObserver(const InputActionObserver&) {}
InputActionObserver& InputActionObserver::operator=(const InputActionObserver&) { return *this; }
InputEvent::Propagation InputActionObserver::OnInputAction(const UId&, const PString&, const InputEvent&)
{
    return InputEvent::PROPAGATE;
}

InputUIExtensionObserver::InputUIExtensionObserver() {}
InputUIExtensionObserver::InputUIExtensionObserver(const InputUIExtensionObserver&) {}
InputUIExtensionObserver& InputUIExtensionObserver::operator=(const InputUIExtensionObserver&) { return *this; }
void InputUIExtensionObserver::OnInputGetEvent() {}
void InputUIExtensionObserver::OnInputLostEvent() {}
InputEvent::Propagation InputUIExtensionObserver::OnKeyboardEvent(const KeyboardEvent&) { return InputEvent::PROPAGATE; }
InputEvent::Propagation InputUIExtensionObserver::OnCharEvent(const CharEvent&) { return InputEvent::PROPAGATE; }
InputEvent::Propagation InputUIExtensionObserver::OnMouseEvent(const MouseEvent&) { return InputEvent::PROPAGATE; }

}  // namespace Jet
